#include "ImGuiPanel.h"

namespace rigkit {

ImGuiPanel::ImGuiPanel(const std::string& title, ImGuiWindowFlags flags)
	: m_title(title), m_windowFlags(flags) {}

void ImGuiPanel::setEngine(RigKitEngine* engine) {
	m_engine = engine;
}

RigKitEngine* ImGuiPanel::getEngine() const {
	return m_engine;
}

bool ImGuiPanel::isVisible() const {
	return m_isVisible;
}

void ImGuiPanel::setVisible(bool visible) {
	m_isVisible = visible;
	if (visible) {
		// Begin(..., &m_isOpen) leaves m_isOpen false after the X button  - 
		// reopen must restore it or showWindow is a no-op visually.
		m_isOpen = true;
		m_focusOnShow = true;
	}
}

void ImGuiPanel::setTitle(const std::string& title) {
	m_title = title;
}

std::string ImGuiPanel::getTitle() const {
	return m_title;
}

bool ImGuiPanel::isOpen() const {
	return m_isOpen;
}

bool ImGuiPanel::isFocused() const {
	return ImGui::IsWindowFocused();
}

std::string ImGuiPanel::getCategory() const {
	return m_category;
}

void ImGuiPanel::setCategory(const std::string& category) {
	m_category = category;
}

void ImGuiPanel::render() {
	if (!m_isVisible) {
		return;
	}
	if (m_focusOnShow) {
		ImGui::SetNextWindowFocus();
		m_focusOnShow = false;
	}
	if (ImGui::Begin(m_title.c_str(), &m_isOpen, m_windowFlags)) {
		const float fh = footerHeight();
		if (fh > 0.f) {
			ImGui::BeginChild("##ImGuiPanel_body", ImVec2(0.f, -fh), ImGuiChildFlags_None,
							  ImGuiWindowFlags_NoScrollbar);
			renderContents();
			ImGui::EndChild();
			renderFooter();
		} else {
			renderContents();
		}
	}
	ImGui::End();
	if (!m_isOpen) {
		m_isVisible = false;
	}
}

void ImGuiPanel::handleInput() {
	// Default implementation - derived classes can override
}

} // namespace rigkit
