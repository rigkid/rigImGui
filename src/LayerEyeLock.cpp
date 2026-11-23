#include "LayerEyeLock.h"

#include <imgui.h>
#include <IconsFontAwesome5.h>

namespace rigkit {

LayerEyeLockEdit drawLayerEyeLock(bool& visible, bool& locked) {
	LayerEyeLockEdit edit;
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 3.f));
	const float side = ImGui::GetFrameHeight();
	const ImVec2 size(side, side);
	const float font = ImGui::GetStyle().FontSizeBase * 0.75f;
	ImGui::PushFont(nullptr, font);

	const char* visIcon = visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
	if (ImGui::Button(visIcon, size)) {
		visible = !visible;
		edit.visibilityToggled = true;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(visible ? "Visible" : "Hidden");
	}
	ImGui::SameLine(0.f, 4.f);

	const char* lockIcon = locked ? ICON_FA_LOCK : ICON_FA_UNLOCK;
	if (ImGui::Button(lockIcon, size)) {
		locked = !locked;
		edit.lockToggled = true;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(locked ? "Locked" : "Unlocked");
	}

	ImGui::PopFont();
	ImGui::PopStyleVar();
	return edit;
}

} // namespace rigkit
