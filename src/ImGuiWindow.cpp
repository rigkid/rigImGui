#include "ImGuiWindow.h"
#include <imgui.h>

namespace rigkit {

ImGuiWindow::ImGuiWindow(const std::string &title, ImGuiWindowFlags flags)
	: IWindow(title, flags) {}

void ImGuiWindow::renderContents() {
	if (m_renderCallback) {
		m_renderCallback();
	} else {
		ImGui::Text("ImGuiWindow base class - override renderContents() in "
					"derived class");
	}
}

void ImGuiWindow::setRenderCallback(std::function<void()> callback) {
	m_renderCallback = callback;
}

} // namespace rigkit
