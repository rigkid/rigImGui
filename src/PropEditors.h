#pragma once

#include <cstdint>
#include <vector>
#include "ecs/PropertyReflection.h"

namespace rigkit {

/**
 * @brief Edit `sProp` fields with ImGui. Returns true if any value changed.
 * @param headerName CollapsingHeader label; null = draw widgets only.
 * @param entityId When non-zero, each row is a drag source (`RIG_SCENE_PROP`) for the Node Editor.
 */
bool RenderProps(const char* headerName, std::vector<sProp>& props, uint32_t entityId = 0);

} // namespace rigkit
