#include "LogWindow.h"
#include <chrono>
#include <imgui.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>
#include <sstream>

namespace rigkit {

// Custom spdlog sink that forwards messages to LogWindow
class LogWindowSink : public spdlog::sinks::base_sink<std::mutex> {
  public:
	LogWindowSink(LogWindow *logWindow) : m_logWindow(logWindow) {}

  protected:
	void sink_it_(const spdlog::details::log_msg &msg) override {
		if (!m_logWindow)
			return;
		// Map spdlog level to LogLevel
		LogLevel level = LogLevel::Info;
		switch (msg.level) {
		case spdlog::level::debug:
			level = LogLevel::Debug;
			break;
		case spdlog::level::info:
			level = LogLevel::Info;
			break;
		case spdlog::level::warn:
			level = LogLevel::Warning;
			break;
		case spdlog::level::err:
			level = LogLevel::Error;
			break;
		default:
			level = LogLevel::Info;
			break;
		}
		std::string message(msg.payload.begin(), msg.payload.end());
		std::string timestamp = m_logWindow->getCurrentTimestamp();
		m_logWindow->addLogFromSink(level, message, timestamp);
	}
	void flush_() override {}

  private:
	LogWindow *m_logWindow;
};

LogWindow::LogWindow(const std::string &title, ImGuiWindowFlags flags)
	: IWindow(title, flags) {
	// Optionally, add an initial log entry via spdlog
	// spdlog::info("Log window initialized.");
}

void LogWindow::setupSpdlogSink() {
	// Create and set a custom sink for this LogWindow
	auto sink = std::make_shared<LogWindowSink>(this);
	setSpdlogSink(sink);
	// Optionally, add this sink to the default logger
	auto logger = spdlog::default_logger();
	if (logger) {
		logger->sinks().push_back(sink);
	}
}

void LogWindow::addLogFromSink(LogLevel level, const std::string &message,
							   const std::string &timestamp) {
	std::lock_guard<std::mutex> lock(m_logMutex);
	if (m_collapseDuplicates) {
		for (size_t i = m_logEntries.size(); i-- > 0;) {
			LogEntry &prev = m_logEntries[i];
			if (prev.level == level && prev.message == message) {
				++prev.count;
				prev.timestamp = timestamp;
				// The row stays where it is, so leave the view where it is too.
				return;
			}
		}
	}
	m_logEntries.emplace_back(level, message, timestamp);
	if (m_logEntries.size() > m_maxLogLines) {
		m_logEntries.erase(m_logEntries.begin());
	}
	m_scrollToBottom = true;
}

void LogWindow::clearLog() {
	std::lock_guard<std::mutex> lock(m_logMutex);
	m_logEntries.clear();
	// Optionally, log this event via spdlog
	spdlog::info("Log cleared.");
}

void LogWindow::dismissEntry(size_t index) {
	std::lock_guard<std::mutex> lock(m_logMutex);
	if (index < m_logEntries.size()) {
		m_logEntries.erase(m_logEntries.begin() + static_cast<std::ptrdiff_t>(index));
	}
}

void LogWindow::renderContents() {
	renderMenuBar();
	renderFilterControls();
	renderLogEntries();
}

void LogWindow::renderMenuBar() {
	if (ImGui::BeginMenuBar()) {
		ImGui::Checkbox("Show Timestamps", &m_showTimestamps);
		ImGui::Checkbox("Collapse Duplicates", &m_collapseDuplicates);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Repeats of a message bump a count on its row\n"
							  "instead of adding another line.");
		}
		ImGui::EndMenuBar();
	}
}

void LogWindow::renderFilterControls() {
	ImGui::Text("Filter:");
	ImGui::SameLine();
	ImGui::Checkbox("Debug", &m_showDebug);
	ImGui::SameLine();
	ImGui::Checkbox("Info", &m_showInfo);
	ImGui::SameLine();
	ImGui::Checkbox("Warning", &m_showWarning);
	ImGui::SameLine();
	ImGui::Checkbox("Error", &m_showError);
	ImGui::SameLine();
	if (ImGui::Button("Clear")) {
		clearLog();
	}
}

void LogWindow::renderLogEntries() {
	// Set black background for log text area only
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 1));
	ImGui::BeginChild("LogEntries", ImVec2(0, 0), true);
	std::unique_lock<std::mutex> lock(m_logMutex);
	// Erasing mid-loop would invalidate the row we are drawing.
	size_t dismiss = m_logEntries.size();
	for (size_t i = 0; i < m_logEntries.size(); ++i) {
		const LogEntry &entry = m_logEntries[i];
		bool shouldShow = false;
		switch (entry.level) {
		case LogLevel::Debug:
			shouldShow = m_showDebug;
			break;
		case LogLevel::Info:
			shouldShow = m_showInfo;
			break;
		case LogLevel::Warning:
			shouldShow = m_showWarning;
			break;
		case LogLevel::Error:
			shouldShow = m_showError;
			break;
		}
		if (!shouldShow)
			continue;
		ImGui::PushID(static_cast<int>(i));
		if (ImGui::SmallButton("x")) {
			dismiss = i;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Dismiss this line");
		}
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, getLogColor(entry.level));
		std::string header;
		if (m_showTimestamps) {
			header = "[" + entry.timestamp + "] [" +
					 getLogLevelString(entry.level) + "] ";
		} else {
			header = "[" + getLogLevelString(entry.level) + "] ";
		}
		if (entry.count > 1) {
			header += "x" + std::to_string(entry.count) + " ";
		}
		ImGui::TextUnformatted(header.c_str());
		ImGui::SameLine();
		ImGui::TextWrapped("%s", entry.message.c_str());
		ImGui::PopStyleColor();
		ImGui::PopID();
	}
	if (dismiss < m_logEntries.size()) {
		lock.unlock();
		dismissEntry(dismiss);
		lock.lock();
	}
	if (m_scrollToBottom) {
		ImGui::SetScrollHereY(1.0f);
		m_scrollToBottom = false;
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

ImVec4 LogWindow::getLogColor(LogLevel level) {
	switch (level) {
	case LogLevel::Debug:
		return ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Gray
	case LogLevel::Info:
		return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White
	case LogLevel::Warning:
		return ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
	case LogLevel::Error:
		return ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
	default:
		return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

std::string LogWindow::getLogLevelString(LogLevel level) {
	switch (level) {
	case LogLevel::Debug:
		return "DEBUG";
	case LogLevel::Info:
		return "INFO";
	case LogLevel::Warning:
		return "WARN";
	case LogLevel::Error:
		return "ERROR";
	default:
		return "UNKNOWN";
	}
}

std::string LogWindow::getCurrentTimestamp() {
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				  now.time_since_epoch()) %
			  1000;
	std::stringstream ss;
	ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
	ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
	return ss.str();
}

} // namespace rigkit
