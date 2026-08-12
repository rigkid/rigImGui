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
	renderStyleEditor();
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

	ImGui::TextUnformatted("Style snapshot");
	ImGui::TextWrapped("Saved under %s (relative names preferred). "
					   "Save / load themes as portable JSON.",
					   AppPaths::getThemesDir().c_str());

	char nameBuf[256];
	std::string& themeFile = ui->uiPrefs().themeFile;
	if (themeFile.empty()) {
		std::snprintf(nameBuf, sizeof(nameBuf), "custom.json");
	} else {
		std::snprintf(nameBuf, sizeof(nameBuf), "%s", themeFile.c_str());
	}
	if (ImGui::InputText("Style file", nameBuf, sizeof(nameBuf))) {
		themeFile = nameBuf;
	}

	if (ImGui::Button(TP_ICON(ICON_FA_SAVE) " Save Style")) {
		ui->saveCurrentTheme(themeFile.empty() ? "custom.json" : themeFile);
	}
	ImGui::SameLine();
	if (ImGui::Button(TP_ICON(ICON_FA_FOLDER_OPEN) " Load Style")) {
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

void ThemePanel::renderStyleEditor() {
	ImGui::TextUnformatted("Live style (Save Style to keep)");
	ImGui::ShowStyleEditor();
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
