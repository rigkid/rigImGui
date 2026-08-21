#pragma once

namespace rigkit {

/** @brief Which part of @ref AnchorPad the user clicked. */
enum class AnchorPadHit { None, Face, Height };

/**
 * @brief 3×3 face pad + optional Z column (4×3).
 * @details Face cells are 0..8 (`rig.spatial.anchor.point`, top-left … bottom-right;
 * top row is max Y). Height cells are 0 min / 1 center / 2 max, drawn
 * top-to-bottom as Max, Center, Min. Pass null to hide that group. Values
 * outside the valid range draw unselected. Click writes the index.
 */
AnchorPadHit AnchorPad(const char* label, int* face, int* height);

} // namespace rigkit
