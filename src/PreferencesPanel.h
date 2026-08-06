#pragma once

#include <string>

#include <imgui.h>
#include "IWindow.h"

namespace rigkit {

/// Catalog-driven editor for MSettings preference sections.
/// Left: category list. Right: options for the selected category.
class PreferencesPanel : public IWindow {
  public:
	PreferencesPanel(const std::string& title = "Preferences",
					 ImGuiWindowFlags flags = 0);
	~PreferencesPanel() override = default;

  private:
	void renderContents() override;

	std::string m_selectedId;
};

} // namespace rigkit
