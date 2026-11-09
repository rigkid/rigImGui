#pragma once

#include "ImGuiPanel.h"

#include <string>

namespace rigkit {

class ShortcutManager;

/** @brief Lists named Kit shortcuts and captures remaps. */
class ShortcutsPanel : public ImGuiPanel {
  public:
	explicit ShortcutsPanel(const std::string& title = "Shortcuts");
	void setShortcutManager(ShortcutManager* shortcuts) { m_shortcuts = shortcuts; }

  protected:
	void renderContents() override;

  private:
	void pollCapture();

	ShortcutManager* m_shortcuts = nullptr;
	std::string m_captureId;
};

} // namespace rigkit
