#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "core/json.h"
#include "imgui.h"

namespace rigkit {

/**
 * @brief One keyboard chord (key + modifiers).
 */
struct ShortcutChord {
	ImGuiKey key = ImGuiKey_None;
	bool ctrl = false;
	bool shift = false;
	bool alt = false;

	bool operator==(const ShortcutChord& o) const {
		return key == o.key && ctrl == o.ctrl && shift == o.shift && alt == o.alt;
	}
	bool operator!=(const ShortcutChord& o) const { return !(*this == o); }
	bool bound() const { return key != ImGuiKey_None; }
};

/**
 * @brief Named keyboard shortcuts for the Kit shell.
 * @details Bindings are id + key chord + action. Remaps persist in
 * `MSettings` under key `shortcuts` (overrides only).
 */
struct ShortcutBinding {
	std::string id;
	std::string label;
	ImGuiKey key = ImGuiKey_None;
	bool ctrl = false;
	bool shift = false;
	bool alt = false;
	std::function<void()> action;

	ShortcutChord chord() const { return {key, ctrl, shift, alt}; }
	void setChord(const ShortcutChord& c) {
		key = c.key;
		ctrl = c.ctrl;
		shift = c.shift;
		alt = c.alt;
	}
};

std::string shortcutChordLabel(const ShortcutChord& c);

class ShortcutManager {
  public:
	void clear();
	void bind(ShortcutBinding binding);
	void unbind(const std::string& id);

	/** @brief Fire matching bindings (skip when capturing or ImGui wants text). */
	void handleInput();

	const std::vector<ShortcutBinding>& bindings() const { return m_bindings; }
	ShortcutBinding* find(const std::string& id);
	const ShortcutBinding* find(const std::string& id) const;

	ShortcutChord defaultChord(const std::string& id) const;
	bool isCustom(const std::string& id) const;

	/**
	 * @brief Remap a binding. Conflicting bindings are cleared (unbound).
	 * @return false if @p id is unknown.
	 */
	bool setChord(const std::string& id, ShortcutChord chord);

	/** @brief Restore factory chord for one binding. */
	bool resetChord(const std::string& id);

	/** @brief Restore factory chords for every binding. */
	void resetAll();

	/** @brief While true, handleInput ignores keys (capture UI owns them). */
	void setCapturing(bool capturing) { m_capturing = capturing; }
	bool capturing() const { return m_capturing; }

	/** @brief Persist callback (Mui writes MSettings). */
	void setOnChanged(std::function<void()> cb) { m_onChanged = std::move(cb); }

	json exportOverrides() const;
	void importOverrides(const json& j);

	/** @brief Display chord for a bound id, or empty. */
	std::string chordLabelFor(const std::string& id) const;

  private:
	void applyStoredOverride(ShortcutBinding& b);
	void notifyChanged();
	void clearConflicts(const std::string& keepId, const ShortcutChord& chord);

	std::vector<ShortcutBinding> m_bindings;
	std::unordered_map<std::string, ShortcutChord> m_defaults;
	std::unordered_map<std::string, ShortcutChord> m_overrides;
	std::function<void()> m_onChanged;
	bool m_capturing = false;
};

} // namespace rigkit
