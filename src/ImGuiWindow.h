#pragma once

#include "ImGuiPanel.h"

#include <functional>

namespace rigkit {

class ImGuiWindow : public ImGuiPanel {
  public:
	ImGuiWindow(const std::string& title, ImGuiWindowFlags flags = 0);
	virtual ~ImGuiWindow() = default;

	// Custom render callback
	void setRenderCallback(std::function<void()> callback);
	void renderContents() override;

  protected:
	// Custom render callback
	std::function<void()> m_renderCallback;
};

} // namespace rigkit
