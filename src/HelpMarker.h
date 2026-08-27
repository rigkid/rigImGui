#pragma once

#include <imgui.h>

namespace rigkit {

/**
 * @brief Disabled "(?)" after a field; hover shows wrapped help.
 * @details Same pattern as imgui_demo HelpMarker. Call after the widget
 * (usually with SameLine). No emoji - plain "(?)".
 */
inline void HelpMarker(const char* desc) {
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip()) {
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

} // namespace rigkit
