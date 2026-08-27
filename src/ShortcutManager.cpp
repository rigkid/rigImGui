#include "ShortcutManager.h"

#include <algorithm>

namespace rigkit {

std::string shortcutChordLabel(const ShortcutChord& c) {
	if (!c.bound()) {
		return "None";
	}
	std::string s;
	if (c.ctrl) {
		s += "Ctrl+";
	}
	if (c.shift) {
		s += "Shift+";
	}
	if (c.alt) {
		s += "Alt+";
	}
	s += ImGui::GetKeyName(c.key);
	return s;
}

void ShortcutManager::clear() {
	m_bindings.clear();
	m_defaults.clear();
	// Keep overrides - they re-apply when bindings return.
}

void ShortcutManager::bind(ShortcutBinding binding) {
	if (binding.id.empty()) {
		return;
	}
	if (m_defaults.find(binding.id) == m_defaults.end()) {
		m_defaults[binding.id] = binding.chord();
	}
	unbind(binding.id);
	applyStoredOverride(binding);
	m_bindings.push_back(std::move(binding));
}

void ShortcutManager::unbind(const std::string& id) {
	m_bindings.erase(std::remove_if(m_bindings.begin(), m_bindings.end(),
									[&](const ShortcutBinding& b) { return b.id == id; }),
					 m_bindings.end());
}

void ShortcutManager::handleInput() {
	if (m_capturing) {
		return;
	}
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantTextInput) {
		return;
	}
	for (const auto& b : m_bindings) {
		if (!b.action || !b.chord().bound()) {
			continue;
		}
		if (b.ctrl != io.KeyCtrl || b.shift != io.KeyShift || b.alt != io.KeyAlt) {
			continue;
		}
		if (ImGui::IsKeyPressed(b.key, false)) {
			b.action();
		}
	}
}

ShortcutBinding* ShortcutManager::find(const std::string& id) {
	for (auto& b : m_bindings) {
		if (b.id == id) {
			return &b;
		}
	}
	return nullptr;
}

const ShortcutBinding* ShortcutManager::find(const std::string& id) const {
	for (const auto& b : m_bindings) {
		if (b.id == id) {
			return &b;
		}
	}
	return nullptr;
}

ShortcutChord ShortcutManager::defaultChord(const std::string& id) const {
	auto it = m_defaults.find(id);
	if (it != m_defaults.end()) {
		return it->second;
	}
	if (const auto* b = find(id)) {
		return b->chord();
	}
	return {};
}

bool ShortcutManager::isCustom(const std::string& id) const {
	return m_overrides.find(id) != m_overrides.end();
}

void ShortcutManager::applyStoredOverride(ShortcutBinding& b) {
	auto it = m_overrides.find(b.id);
	if (it != m_overrides.end()) {
		b.setChord(it->second);
	}
}

void ShortcutManager::clearConflicts(const std::string& keepId, const ShortcutChord& chord) {
	if (!chord.bound()) {
		return;
	}
	for (auto& b : m_bindings) {
		if (b.id == keepId || b.chord() != chord) {
			continue;
		}
		b.setChord({});
		m_overrides[b.id] = {};
	}
}

bool ShortcutManager::setChord(const std::string& id, ShortcutChord chord) {
	auto* b = find(id);
	if (!b) {
		return false;
	}
	clearConflicts(id, chord);
	b->setChord(chord);
	const ShortcutChord def = defaultChord(id);
	if (chord == def) {
		m_overrides.erase(id);
	} else {
		m_overrides[id] = chord;
	}
	notifyChanged();
	return true;
}

bool ShortcutManager::resetChord(const std::string& id) {
	auto it = m_defaults.find(id);
	if (it == m_defaults.end()) {
		return false;
	}
	return setChord(id, it->second);
}

void ShortcutManager::resetAll() {
	bool any = false;
	for (auto& b : m_bindings) {
		auto it = m_defaults.find(b.id);
		if (it == m_defaults.end()) {
			continue;
		}
		if (b.chord() != it->second) {
			b.setChord(it->second);
			any = true;
		}
	}
	if (!m_overrides.empty()) {
		m_overrides.clear();
		any = true;
	}
	if (any) {
		notifyChanged();
	}
}

void ShortcutManager::notifyChanged() {
	if (m_onChanged) {
		m_onChanged();
	}
}

json ShortcutManager::exportOverrides() const {
	json out = json::object();
	for (const auto& [id, c] : m_overrides) {
		out[id] = json{{"key", static_cast<int>(c.key)},
					   {"ctrl", c.ctrl},
					   {"shift", c.shift},
					   {"alt", c.alt}};
	}
	return out;
}

void ShortcutManager::importOverrides(const json& j) {
	m_overrides.clear();
	if (!j.is_object()) {
		return;
	}
	for (auto it = j.begin(); it != j.end(); ++it) {
		if (!it.value().is_object()) {
			continue;
		}
		const auto& o = it.value();
		ShortcutChord c;
		c.key = static_cast<ImGuiKey>(o.value("key", static_cast<int>(ImGuiKey_None)));
		c.ctrl = o.value("ctrl", false);
		c.shift = o.value("shift", false);
		c.alt = o.value("alt", false);
		m_overrides[it.key()] = c;
	}
	for (auto& b : m_bindings) {
		auto def = m_defaults.find(b.id);
		if (def != m_defaults.end()) {
			b.setChord(def->second);
		}
		applyStoredOverride(b);
	}
	// Resolve conflicts after load (keep first binding order).
	for (size_t i = 0; i < m_bindings.size(); ++i) {
		const auto chord = m_bindings[i].chord();
		if (!chord.bound()) {
			continue;
		}
		for (size_t j = i + 1; j < m_bindings.size(); ++j) {
			if (m_bindings[j].chord() == chord) {
				m_bindings[j].setChord({});
				m_overrides[m_bindings[j].id] = {};
			}
		}
	}
}

std::string ShortcutManager::chordLabelFor(const std::string& id) const {
	if (const auto* b = find(id)) {
		return shortcutChordLabel(b->chord());
	}
	return {};
}

} // namespace rigkit
