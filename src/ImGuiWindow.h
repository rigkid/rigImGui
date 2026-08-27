#pragma once

#include "IWindow.h"

#include <functional>

namespace rigkit {

class ImGuiWindow : public IWindow {
  public:
	ImGuiWindow(const std::string &title, ImGuiWindowFlags flags = 0);
	virtual ~ImGuiWindow() = default;

	// Custom render callback
	void setRenderCallback(std::function<void()> callback);
	void renderContents() override;

  protected:
	// Custom render callback
	std::function<void()> m_renderCallback;
};

} // namespace rigkit
