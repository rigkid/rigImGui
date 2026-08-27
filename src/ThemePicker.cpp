#include "ThemePicker.h"

#include "ImGuiStyleKit.h"

#include <imgui.h>

namespace rigkit {
namespace ThemePicker {

bool draw(const char* strId, std::string& themeFile) {
	bool changed = false;
	ImGui::PushID(strId);

	const auto files = ImGuiStyleKit::listThemeFiles();
	const ThemeFile* current = ImGuiStyleKit::findThemeFile(files, themeFile);
	const char* preview = themeFile.empty()
							  ? "None"
							  : (current ? current->name.c_str() : themeFile.c_str());

	if (ImGui::BeginCombo("Color scheme", preview)) {
		if (ImGui::Selectable("None", themeFile.empty())) {
			if (!themeFile.empty()) {
				themeFile.clear();
				changed = true;
			}
		}
		for (const auto& f : files) {
			const bool sel = current == &f;
			if (ImGui::Selectable(f.name.c_str(), sel)) {
				if (themeFile != f.fileName) {
					themeFile = f.fileName;
					changed = true;
				}
			}
			if (sel) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (current) {
		if (!current->credit.empty()) {
			ImGui::TextDisabled("Credit: %s", current->credit.c_str());
		}
		if (!current->license.empty()) {
			ImGui::TextDisabled("License: %s", current->license.c_str());
		}
		if (!current->source.empty()) {
			ImGui::TextWrapped("%s", current->source.c_str());
		}
	}

	ImGui::PopID();
	return changed;
}

} // namespace ThemePicker
} // namespace rigkit
