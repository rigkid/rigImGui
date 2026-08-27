#pragma once

#include "IWindow.h"

#include <imgui.h>

namespace rigkit {

class ThemePanel : public IWindow {
  public:
	ThemePanel(const std::string& title = "Theme Panel###ThemePanel",
			   ImGuiWindowFlags flags = 0);
	~ThemePanel() override = default;

	void renderContents() override;

  private:
	void renderThemeControls();
	void renderThemeFileControls();
	void renderFontControls();
	void renderStyleEditor();
};

} // namespace rigkit
