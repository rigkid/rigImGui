#pragma once

#include "ImGuiPanel.h"

#include <entt/entt.hpp>

namespace rigkit {

/**
 * @brief Flat ECS entity browser. Selection drives Properties via CSelection.
 */
class EntitiesWindow : public ImGuiPanel {
  public:
	explicit EntitiesWindow(const std::string& title = "Entities");

  protected:
	void renderContents() override;
};

} // namespace rigkit
