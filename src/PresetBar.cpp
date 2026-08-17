#include "PresetBar.h"

#include <cstdio>
#include <imgui.h>

namespace rigkit {
namespace PresetBar {

Result draw(const char* strId, const char* label, const std::vector<std::string>& names,
			std::string& name, bool dirty, const char* hint) {
	Result r;
	ImGui::PushID(strId);

	int currentIdx = -1;
	for (int i = 0; i < static_cast<int>(names.size()); ++i) {
		if (names[static_cast<size_t>(i)] == name) {
			currentIdx = i;
			break;
		}
	}

	std::string preview = name.empty() ? "(none)" : name;
	if (dirty && !name.empty()) {
		preview += "*";
	}

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(160.f);
	if (ImGui::BeginCombo("##preset", preview.c_str())) {
		for (int i = 0; i < static_cast<int>(names.size()); ++i) {
			const auto& n = names[static_cast<size_t>(i)];
			const bool selected = (i == currentIdx);
			if (ImGui::Selectable(n.c_str(), selected)) {
				r.action = Action::Load;
				r.name = n;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::IsItemHovered() && dirty && !name.empty()) {
		ImGui::SetTooltip("Differs from the named preset — Save to overwrite.");
	}

	ImGui::SameLine();
	char buf[128];
	std::snprintf(buf, sizeof(buf), "%s", name.c_str());
	ImGui::SetNextItemWidth(120.f);
	if (ImGui::InputTextWithHint("##name", "Name", buf, sizeof(buf))) {
		name = buf;
		r.nameEdited = true;
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(name.empty());
	if (ImGui::Button("Save")) {
		r.action = Action::Save;
		r.name = name;
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(currentIdx < 0);
	if (ImGui::Button("Delete")) {
		r.action = Action::Delete;
		r.name = name;
	}
	ImGui::EndDisabled();

	if (hint && hint[0] != '\0') {
		ImGui::TextDisabled("%s", hint);
	}

	ImGui::PopID();
	return r;
}

} // namespace PresetBar
} // namespace rigkit
