#pragma once

#include "ecs/MEcs.h"

#include <functional>

namespace rigkit {

/** @brief Optional hooks while drawing a `rig.ui.panel` of controls. */
struct UiControlDrawHost {
	std::function<void()> onChanged;
	std::function<void(entt::entity)> unpublish;
};

/**
 * @brief Draw every `CUiControl` on the first panel whose `role` matches.
 * @return Number of controls drawn (0 if no panel / no controls).
 */
int drawUiPanelControls(MEcs& ecs, const char* role, const UiControlDrawHost& host = {});

} // namespace rigkit
