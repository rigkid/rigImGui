#pragma once

#include "ImGuiPanel.h"

#include <entt/entt.hpp>

namespace rigkit {

/**
 * @brief Entity list / hierarchy over CRelationship + CSelection.
 */
class SceneWindow : public ImGuiPanel {
  public:
	explicit SceneWindow(const std::string& title = "Scene");

  protected:
	void renderContents() override;

  private:
	void selectOnly(entt::entity e);
};

} // namespace rigkit
