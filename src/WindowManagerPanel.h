#pragma once
#include <memory>
#include <string>
#include <vector>

#include "ImGuiPanel.h"
#include "MWindow.h"

namespace rigkit {

class WindowManagerPanel : public ImGuiPanel {
  public:
	WindowManagerPanel(const std::string& title, MWindow* windowManager,
					   ImGuiWindowFlags flags = 0);

  protected:
	void renderContents() override;

  private:
	MWindow* m_windowManager;
};

} // namespace rigkit
