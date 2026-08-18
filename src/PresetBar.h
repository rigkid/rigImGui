#pragma once

#include <string>
#include <vector>

namespace rigkit {
namespace PresetBar {

enum class Action { None, Load, Save, Delete };

struct Result {
	Action action = Action::None;
	std::string name;
	bool nameEdited = false;
};

/**
 * @brief Names dropdown + Save + Delete for named disk presets.
 * @details Combo pick is Load. Save opens a name popup (pre-filled with
 * @p name); confirm sets Action::Save. Delete uses the current @p name when
 * that stem is in @p names. Dirty marks the combo preview with *. Same row
 * everywhere — callers only do I/O. No inline name field.
 * @param label Left-side caption ("Preset" / "Snippet").
 * @param name Current stem; updated on Load and on confirmed Save.
 * @param hint Optional disabled line under the row.
 */
Result draw(const char* strId, const char* label, const std::vector<std::string>& names,
			std::string& name, bool dirty, const char* hint = nullptr);

} // namespace PresetBar
} // namespace rigkit
