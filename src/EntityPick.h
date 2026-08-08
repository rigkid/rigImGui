#pragma once

#include "ecs/MEcs.h"
#include <entt/entt.hpp>

namespace rigkit {

/**
 * @brief Pick nearest selectable geometry entity under a content-space point.
 * @details Honors `CSelectable::enabled` when present; absence = legacy selectable.
 * @return entity or entt::null.
 */
entt::entity pickEntityAt(MEcs& ecs, float contentX, float contentY, float maxDist = 8.f);

/** @brief Clear multi-select and select only `e` (adds CSelection if needed). */
void selectEntityOnly(MEcs& ecs, entt::entity e);

} // namespace rigkit
