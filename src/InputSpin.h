#pragma once

namespace rigkit {

/**
 * @brief Integer field with + stacked on - .
 * @details Each button is half the frame height, so the pair is the same
 * height as the field and one button wide. Hold to repeat. Ctrl applies
 * @p stepFast. Does not clamp; the caller keeps the range. The label is
 * drawn to the right of the buttons, same place as ImGui::InputInt.
 * Does not draw the side-by-side full-height pair.
 * @param step Added on +, subtracted on -. Zero is treated as 1.
 * @param stepFast Used instead of @p step while Ctrl is held. Zero skips that.
 * @return true when the value changes.
 */
bool InputSpinInt(const char* label, int* v, int step = 1, int stepFast = 100);

} // namespace rigkit
