#pragma once

#include "ecs/MEcs.h"

namespace rigkit {

/**
 * @brief Draw/manipulate a 2D bbox for the first selected geometry entity.
 * @return true if a handle was drawn.
 */
bool drawSelectedHandle2D(MEcs& ecs, float originX, float originY, float scale);

} // namespace rigkit
