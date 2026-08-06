#pragma once

#include <imgui.h>

namespace rigkit {

enum class DropZone { Before, Into, After };

/** @brief Result of an index-based list reorder drop. */
struct IndexDropResult {
	bool accepted = false;
	int dragged = -1;
	int target = -1;
	DropZone zone = DropZone::Before;
};

/**
 * @brief Flat-list drag-reorder (pipeline steps, effect chains).
 * @details Call immediately after the row's interactive ImGui item.
 * Before/After zones use the top/bottom thirds of [rowMinY, rowMaxY].
 */
IndexDropResult ReorderDragDropIndexRow(const char* payloadTag, int index, const char* previewLabel,
										float rowMinY, float rowMaxY);

} // namespace rigkit
