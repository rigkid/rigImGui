#include "ThemePanel.h"
#include <cstdio>
#include <cstdlib>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <string>
#include "FontPicker.h"
#include "ImGuiStyleKit.h"
#include "Mui.h"
#include "core/IMui.h"
#include "core/RigKitEngine.h"
#include "core/util/AppPaths.h"
#include "core/util/MSettings.h"

#if __has_include("IconsFontAwesome5.h")
#include "IconsFontAwesome5.h"
#define TP_ICON(x) x
#else
#define TP_ICON(x) ""
#endif

namespace rigkit {

ThemePanel::ThemePanel(const std::string& title, ImGuiWindowFlags flags)
	: IWindow(title, flags) {}

void ThemePanel::renderContents() {
	renderThemeControls();
	ImGui::Separator();
	renderThemeFileControls();
	ImGui::Separator();
	renderFontControls();
	ImGui::Separator();
	renderColorPresets();
	ImGui::Separator();
	renderRandomThemeButton();
}

void ThemePanel::renderThemeControls() {
	ImGui::TextUnformatted("Built-in themes");
	Mui* ui = nullptr;
	if (getEngine() && getEngine()->getUiManager()) {
		ui = dynamic_cast<Mui*>(getEngine()->getUiManager());
	}

	const int current = ui ? static_cast<int>(ui->getImGuiTheme()) : -1;

	if (ImGui::Button(TP_ICON(ICON_FA_MOON) " Dark")) {
		if (ui) {
			ui->uiPrefs().themeFile.clear();
			ui->setImGuiTheme(ImGuiTheme::Dark);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button(TP_ICON(ICON_FA_SUN) " Light")) {
		if (ui) {
			ui->uiPrefs().themeFile.clear();
			ui->setImGuiTheme(ImGuiTheme::Light);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button(TP_ICON(ICON_FA_PAINT_BRUSH) " Classic")) {
		if (ui) {
			ui->uiPrefs().themeFile.clear();
			ui->setImGuiTheme(ImGuiTheme::Classic);
		}
	}

	if (ImGui::Button("Corporate")) {
		if (ui) {
			ui->uiPrefs().themeFile.clear();
			ui->setImGuiTheme(ImGuiTheme::Corporate);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Dracula")) {
		if (ui) {
			ui->uiPrefs().themeFile.clear();
			ui->setImGuiTheme(ImGuiTheme::Dracula);
		}
	}

	if (current >= 0) {
		ImGui::TextDisabled("Current built-in id: %d", current);
	}
}

void ThemePanel::renderThemeFileControls() {
	Mui* ui = nullptr;
	if (getEngine() && getEngine()->getUiManager()) {
		ui = dynamic_cast<Mui*>(getEngine()->getUiManager());
	}
	if (!ui) {
		return;
	}

	ImGui::TextUnformatted("Custom theme file");
	ImGui::TextWrapped("Saved under %s (relative names preferred).",
					   AppPaths::getThemesDir().c_str());

	char nameBuf[256];
	std::string& themeFile = ui->uiPrefs().themeFile;
	if (themeFile.empty()) {
		std::snprintf(nameBuf, sizeof(nameBuf), "custom.json");
	} else {
		std::snprintf(nameBuf, sizeof(nameBuf), "%s", themeFile.c_str());
	}
	if (ImGui::InputText("Theme file", nameBuf, sizeof(nameBuf))) {
		themeFile = nameBuf;
	}

	if (ImGui::Button(TP_ICON(ICON_FA_SAVE) " Save Theme")) {
		ui->saveCurrentTheme(themeFile.empty() ? "custom.json" : themeFile);
	}
	ImGui::SameLine();
	if (ImGui::Button(TP_ICON(ICON_FA_FOLDER_OPEN) " Load Theme")) {
		ui->loadTheme(themeFile.empty() ? "custom.json" : themeFile);
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear File")) {
		themeFile.clear();
		ui->setImGuiTheme(ui->getImGuiTheme());
	}
}

void ThemePanel::renderFontControls() {
	Mui* ui = nullptr;
	if (getEngine() && getEngine()->getUiManager()) {
		ui = dynamic_cast<Mui*>(getEngine()->getUiManager());
	}
	if (!ui) {
		return;
	}

	ImGui::TextUnformatted("UI font");
	FontPicker::draw("themeFont", ui->uiPrefs().fontFile, &ui->uiPrefs().fontSize, ui);

	if (ImGui::Button(TP_ICON(ICON_FA_SYNC) " Apply Font")) {
		if (ui->reloadFonts()) {
			ui->showNotification("Font applied", NotificationType::Success);
		} else {
			ui->showNotification("Font apply failed — check path", NotificationType::Error);
		}
		if (auto* engine = getEngine()) {
			if (auto* settings = engine->getSettingsManager()) {
				settings->markDirty();
			}
		}
	}
}

void ThemePanel::renderColorPresets() {
	ImGui::TextUnformatted("Live style (edits current session — Save Theme to keep)");
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	if (ImGui::CollapsingHeader("Text Colors")) {
		ImGui::ColorEdit4("Text", (float*)&colors[ImGuiCol_Text]);
		ImGui::ColorEdit4("Text Disabled", (float*)&colors[ImGuiCol_TextDisabled]);
	}

	if (ImGui::CollapsingHeader("Background Colors")) {
		ImGui::ColorEdit4("Window Background", (float*)&colors[ImGuiCol_WindowBg]);
		ImGui::ColorEdit4("Child Background", (float*)&colors[ImGuiCol_ChildBg]);
		ImGui::ColorEdit4("Popup Background", (float*)&colors[ImGuiCol_PopupBg]);
	}

	if (ImGui::CollapsingHeader("Button Colors")) {
		ImGui::ColorEdit4("Button", (float*)&colors[ImGuiCol_Button]);
		ImGui::ColorEdit4("Button Hovered", (float*)&colors[ImGuiCol_ButtonHovered]);
		ImGui::ColorEdit4("Button Active", (float*)&colors[ImGuiCol_ButtonActive]);
	}

	if (ImGui::CollapsingHeader("Frame Colors")) {
		ImGui::ColorEdit4("Frame Background", (float*)&colors[ImGuiCol_FrameBg]);
		ImGui::ColorEdit4("Frame Hovered", (float*)&colors[ImGuiCol_FrameBgHovered]);
		ImGui::ColorEdit4("Frame Active", (float*)&colors[ImGuiCol_FrameBgActive]);
	}

	if (ImGui::CollapsingHeader("Metrics")) {
		ImGui::SliderFloat("Window Rounding", &style.WindowRounding, 0.0f, 12.0f);
		ImGui::SliderFloat("Frame Rounding", &style.FrameRounding, 0.0f, 12.0f);
		ImGui::SliderFloat("Grab Rounding", &style.GrabRounding, 0.0f, 12.0f);
		ImGui::SliderFloat2("Window Padding", (float*)&style.WindowPadding, 0.0f, 24.0f);
		ImGui::SliderFloat2("Frame Padding", (float*)&style.FramePadding, 0.0f, 20.0f);
		ImGui::SliderFloat2("Item Spacing", (float*)&style.ItemSpacing, 0.0f, 20.0f);
	}
}

void ThemePanel::renderRandomThemeButton() {
	if (ImGui::Button(TP_ICON(ICON_FA_PALETTE) " Random Theme (play)")) {
		auto& style = ImGui::GetStyle();
		auto& colors = style.Colors;
		auto randomColor = []() {
			return ImVec4(static_cast<float>(rand()) / RAND_MAX,
						  static_cast<float>(rand()) / RAND_MAX,
						  static_cast<float>(rand()) / RAND_MAX, 1.0f);
		};
		colors[ImGuiCol_Button] = randomColor();
		colors[ImGuiCol_Header] = randomColor();
		colors[ImGuiCol_CheckMark] = randomColor();
		spdlog::info("[ThemePanel] Applied playful random accents");
	}
}

} // namespace rigkit
