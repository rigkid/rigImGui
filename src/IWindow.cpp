#include "IWindow.h"

namespace rigkit {

IWindow::IWindow(const std::string& title, ImGuiWindowFlags flags)
	: m_title(title), m_windowFlags(flags) {}

void IWindow::setEngine(RigKitEngine* engine) {
	m_engine = engine;
}

RigKitEngine* IWindow::getEngine() const {
	return m_engine;
}

bool IWindow::isVisible() const {
	return m_isVisible;
}

void IWindow::setVisible(bool visible) {
	m_isVisible = visible;
	if (visible) {
		// Begin(..., &m_isOpen) leaves m_isOpen false after the X button —
		// reopen must restore it or showWindow is a no-op visually.
		m_isOpen = true;
		m_focusOnShow = true;
	}
}

void IWindow::setTitle(const std::string& title) {
	m_title = title;
}

std::string IWindow::getTitle() const {
	return m_title;
}

bool IWindow::isOpen() const {
	return m_isOpen;
}

bool IWindow::isFocused() const {
	return ImGui::IsWindowFocused();
}

std::string IWindow::getCategory() const {
	return m_category;
}

void IWindow::setCategory(const std::string& category) {
	m_category = category;
}

void IWindow::render() {
	if (!m_isVisible) {
		return;
	}
	if (m_focusOnShow) {
		ImGui::SetNextWindowFocus();
		m_focusOnShow = false;
	}
	if (ImGui::Begin(m_title.c_str(), &m_isOpen, m_windowFlags)) {
		renderContents();
	}
	ImGui::End();
	if (!m_isOpen) {
		m_isVisible = false;
	}
}

void IWindow::handleInput() {
	// Default implementation - derived classes can override
}

} // namespace rigkit
