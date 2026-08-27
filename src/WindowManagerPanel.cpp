#include "WindowManagerPanel.h"

#include <imgui.h>

namespace rigkit {

WindowManagerPanel::WindowManagerPanel(const std::string &title,
									   MWindow *windowManager,
									   ImGuiWindowFlags flags)
	: IWindow(title, flags), m_windowManager(windowManager) {}

void WindowManagerPanel::renderContents() {
	if (!m_windowManager) {
		ImGui::Text("Window management not available");
		return;
	}
	auto windowNames = m_windowManager->getAllWindowNames();
	for (const auto &win : windowNames) {
		bool isVisible = m_windowManager->getWindowVisibility(win);
		if (ImGui::Checkbox(win.c_str(), &isVisible)) {
			m_windowManager->setWindowVisible(win, isVisible);
		}
	}
	if (windowNames.empty()) {
		ImGui::Text("No windows found");
	}
}

} // namespace rigkit
