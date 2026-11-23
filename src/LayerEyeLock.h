#pragma once

namespace rigkit {

/** @brief Which eye / lock button was clicked this frame. */
struct LayerEyeLockEdit {
	bool visibilityToggled = false;
	bool lockToggled = false;
};

/**
 * @brief Eye and lock buttons for one layer row.
 * @details Flips @p visible and @p locked when that button is clicked.
 * Does not record undo and does not read a document.
 */
LayerEyeLockEdit drawLayerEyeLock(bool& visible, bool& locked);

} // namespace rigkit
