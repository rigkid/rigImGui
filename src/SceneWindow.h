#pragma once

#include "IWindow.h"
#include <entt/entt.hpp>

namespace rigkit {

/**
 * @brief Entity list / hierarchy over CRelationship + CSelection.
 */
class SceneWindow : public IWindow {
  public:
	explicit SceneWindow(const std::string& title = "Scene");

  protected:
	void renderContents() override;

  private:
	void selectOnly(entt::entity e);
};

} // namespace rigkit

