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
 * @brief Combo + name + Save + Delete for named disk presets.
 * @details Combo pick is Load. Save uses @p name (disabled when empty).
 * Delete uses @p name when that stem is in @p names. Dirty marks the
 * combo preview with *. Same row everywhere — callers only do I/O.
 * @param label Left-side caption ("Preset").
 * @param name Current / save-as stem; InputText writes back here.
 * @param hint Optional disabled line under the row.
 */
Result draw(const char* strId, const char* label, const std::vector<std::string>& names,
			std::string& name, bool dirty, const char* hint = nullptr);

} // namespace PresetBar
} // namespace rigkit
