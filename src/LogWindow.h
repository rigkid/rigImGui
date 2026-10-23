#pragma once

#include "IWindow.h"

#include <imgui.h>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace spdlog {
class logger;
namespace sinks {
class sink;
}
} // namespace spdlog

namespace rigkit {

enum class LogLevel { Debug, Info, Warning, Error };

struct LogEntry {
	LogLevel level;
	std::string message;
	std::string timestamp; ///< Of the most recent occurrence.
	int count = 1;		   ///< Occurrences collapsed into this row.
	LogEntry(LogLevel lvl, const std::string& msg, const std::string& time = "")
		: level(lvl), message(msg), timestamp(time) {}
};

class LogWindow : public IWindow {
  public:
	LogWindow(const std::string& title = "Log###Log", ImGuiWindowFlags flags = 0);
	~LogWindow() override;

	// spdlog sink integration
	void setupSpdlogSink();
	void setSpdlogSink(std::shared_ptr<spdlog::sinks::sink> sink) { m_spdlogSink = sink; }
	std::shared_ptr<spdlog::sinks::sink> getSpdlogSink() const { return m_spdlogSink; }

	// For UI: clear log buffer
	void clearLog();

	/**
	 * @brief Drop one row of the live buffer. A collapsed row takes its repeats with it.
	 * @param index Index into the live buffer, not the on-screen snapshot.
	 */
	void dismissEntry(size_t index);

	// Ensure this class is not abstract
	void renderContents() override;

	friend class LogWindowSink;
	friend class StdTeeBuf;

  private:
	std::vector<LogEntry> m_logEntries;
	std::mutex m_logMutex;
	// UI thread only. Drawn without m_logMutex: holding that lock across ImGui
	// deadlocks when this thread logs again, or when a worker sits in the
	// spdlog sink waiting for the same mutex.
	std::vector<LogEntry> m_drawEntries;
	std::vector<size_t> m_visibleRows;
	uint64_t m_revision = 0;
	uint64_t m_drawnRevision = 0;
	bool m_scrollToBottom = true;
	bool m_showDebug = true;
	bool m_showInfo = true;
	bool m_showWarning = true;
	bool m_showError = true;
	bool m_showTimestamps = true; // Toggle for timestamp display
	// A repeated message becomes a count on the row it already has, so one
	// warning per rebuild does not push the rest of the log out of the buffer.
	bool m_collapseDuplicates = true;
	size_t m_maxLogLines = 1000;
	std::shared_ptr<spdlog::sinks::sink> m_spdlogSink;
	std::streambuf* m_coutBuf = nullptr;
	std::streambuf* m_cerrBuf = nullptr;
	std::unique_ptr<std::streambuf> m_teeOut;
	std::unique_ptr<std::streambuf> m_teeErr;

	void captureStdStreams();
	void releaseStdStreams();

	// UI methods
	void renderLogEntries();
	void renderFilterControls();
	void renderMenuBar();
	void dismissDrawn(size_t index);
	bool showsLevel(LogLevel level) const;
	ImVec4 getLogColor(LogLevel level);
	std::string getLogLevelString(LogLevel level);

  protected:
	// Methods needed by LogWindowSink
	void addLogFromSink(LogLevel level, const std::string& message, const std::string& timestamp);
	std::string getCurrentTimestamp();
};

} // namespace rigkit
