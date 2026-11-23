#pragma once

#include <glm/vec4.hpp>
#include <imgui.h>

namespace rigkit {

/**
 * @brief Edits reported by one draw of the stacked colour chips.
 */
struct DualColorEdit {
	bool swapped = false;
	int picked = -1; ///< 0 = front, 1 = back, when the picker changed that colour.
};

struct DualColorSlotsOpts {
	/** @brief Double-click a chip to open a colour picker. */
	bool enablePicker = true;
};

/**
 * @brief Stacked front and back colour chips.
 * @details Click selects the active slot. The corner disc swaps the two colours.
 * Double-click opens a picker when @p opts.enablePicker. Does not apply the
 * colours to paths, brushes, or a document.
 * @param front Top-left chip. Written in place.
 * @param back Bottom-right chip. Written in place.
 * @param active 0 selects front, 1 selects back. Written in place.
 * @param size Hit box in pixels.
 * @param frontLabel Tooltip and picker title for @p front.
 * @param backLabel Tooltip and picker title for @p back.
 * @return Swap and picker edits from this frame.
 */
DualColorEdit drawDualColorSlots(glm::vec4& front, glm::vec4& back, int& active, ImVec2 size,
								 const char* frontLabel, const char* backLabel,
								 const DualColorSlotsOpts& opts = {});

} // namespace rigkit
