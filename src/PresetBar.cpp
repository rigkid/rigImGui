#include "PresetBar.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <string>
#include <unordered_map>

namespace rigkit {
namespace PresetBar {
namespace {

struct SaveDraft {
	char buf[128] = {};
};

SaveDraft& draftFor(const char* strId) {
	static std::unordered_map<std::string, SaveDraft> drafts;
	return drafts[strId];
}

} // namespace

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
	const ImGuiStyle& st = ImGui::GetStyle();
	const float saveW = ImGui::CalcTextSize("Save").x + st.FramePadding.x * 2.f;
	const float deleteW = ImGui::CalcTextSize("Delete").x + st.FramePadding.x * 2.f;
	const float spacing = st.ItemSpacing.x;
	ImGui::SetNextItemWidth(
		std::max(48.f, ImGui::GetContentRegionAvail().x - saveW - deleteW - spacing * 2.f));
	if (ImGui::BeginCombo("##preset", preview.c_str())) {
		for (int i = 0; i < static_cast<int>(names.size()); ++i) {
			const auto& n = names[static_cast<size_t>(i)];
			const bool selected = (i == currentIdx);
			if (ImGui::Selectable(n.c_str(), selected)) {
				r.action = Action::Load;
				r.name = n;
				name = n;
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

	ImGui::SameLine(0.f, spacing);
	if (ImGui::Button("Save", ImVec2(saveW, 0.f))) {
		ImGui::OpenPopup("##save_preset_name");
	}

	ImGui::SameLine(0.f, spacing);
	ImGui::BeginDisabled(currentIdx < 0);
	if (ImGui::Button("Delete", ImVec2(deleteW, 0.f))) {
		r.action = Action::Delete;
		r.name = name;
	}
	ImGui::EndDisabled();

	if (ImGui::BeginPopupModal("##save_preset_name", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize)) {
		SaveDraft& draft = draftFor(strId);
		if (ImGui::IsWindowAppearing()) {
			std::snprintf(draft.buf, sizeof(draft.buf), "%s", name.c_str());
		}
		ImGui::TextUnformatted("Preset name");
		ImGui::SetNextItemWidth(280.f);
		const bool enter = ImGui::InputText("##save_name", draft.buf, sizeof(draft.buf),
											ImGuiInputTextFlags_EnterReturnsTrue);
		const bool canSave = draft.buf[0] != '\0';
		ImGui::BeginDisabled(!canSave);
		if ((ImGui::Button("OK", ImVec2(120.f, 0.f)) || (enter && canSave))) {
			name = draft.buf;
			r.action = Action::Save;
			r.name = name;
			r.nameEdited = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.f, 0.f))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (hint && hint[0] != '\0') {
		ImGui::TextDisabled("%s", hint);
	}

	ImGui::PopID();
	return r;
}

} // namespace PresetBar
} // namespace rigkit
