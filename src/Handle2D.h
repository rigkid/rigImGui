#pragma once

#include "core/IMui.h"
#include "ecs/MEcs.h"

namespace rigkit {
class UndoStack;

/**
 * @brief Draw/manipulate a 2D bbox for the first selected geometry or text entity.
 * @details @p origin / @p scale map content units to screen (View2D ox/oy + zoomAbs).
 * Draw on the background list so handles stay under menus.
 * @return true if a handle was drawn.
 */
bool drawSelectedHandle2D(MEcs& ecs, float originX, float originY, float scale,
						  IMui::GizmoOp op = IMui::GizmoOp::Select,
						  UndoStack* undo = nullptr);

} // namespace rigkit
