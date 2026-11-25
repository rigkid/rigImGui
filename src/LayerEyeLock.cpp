#include "LayerEyeLock.h"

#include "UiDpi.h"

#include <imgui.h>
#include <IconsFontAwesome5.h>

namespace rigkit {

LayerEyeLockEdit drawLayerEyeLock(bool& visible, bool& locked) {
	LayerEyeLockEdit edit;
	const float font = ImGui::GetStyle().FontSizeBase * 0.75f;
	ImGui::PushFont(nullptr, font);

	const char* visIcon = visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
	if (uiIconButton(visIcon)) {
		visible = !visible;
		edit.visibilityToggled = true;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(visible ? "Visible" : "Hidden");
	}
	ImGui::SameLine(0.f, ImGui::GetStyle().ItemInnerSpacing.x);

	const char* lockIcon = locked ? ICON_FA_LOCK : ICON_FA_UNLOCK;
	if (uiIconButton(lockIcon)) {
		locked = !locked;
		edit.lockToggled = true;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(locked ? "Locked" : "Unlocked");
	}

	ImGui::PopFont();
	return edit;
}

} // namespace rigkit
