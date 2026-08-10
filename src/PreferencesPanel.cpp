#include "PreferencesPanel.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>
#include "FontPicker.h"
#include "Mui.h"
#include "PropEditors.h"
#include "UiDpi.h"
#include "core/RigKitEngine.h"
#include "core/util/AppPaths.h"
#include "core/util/MSettings.h"

namespace rigkit {
namespace {

struct PrefCategory {
	std::string id;
	std::string label;
	bool isDrawer = false;
};

const PrefCategory* findCategory(const std::vector<PrefCategory>& categories,
								 const std::string& id) {
	for (const auto& cat : categories) {
		if (cat.id == id) {
			return &cat;
		}
	}
	return nullptr;
}

} // namespace

PreferencesPanel::PreferencesPanel(const std::string& title, ImGuiWindowFlags flags)
	: IWindow(title, flags != 0 ? flags : ImGuiWindowFlags_NoDocking) {}

void PreferencesPanel::renderContents() {
	auto* engine = getEngine();
	if (!engine) {
		ImGui::TextDisabled("No engine");
		return;
	}
	auto* settings = engine->getSettingsManager();
	if (!settings) {
		ImGui::TextDisabled("No settings manager");
		return;
	}

	auto* mui = dynamic_cast<Mui*>(engine->getUiManager());
	std::vector<PrefCategory> categories;
	if (mui) {
		for (const auto& drawer : mui->preferencesDrawers()) {
			categories.push_back({drawer.id, drawer.label, true});
		}
	}
	for (const auto& section : settings->preferenceSections()) {
		categories.push_back({section.id, section.label, false});
	}
	std::sort(categories.begin(), categories.end(),
			  [](const PrefCategory& a, const PrefCategory& b) {
				  return a.label < b.label;
			  });

	if (categories.empty()) {
		ImGui::TextDisabled("No preference sections registered");
		return;
	}

	if (!findCategory(categories, m_selectedId)) {
		m_selectedId = categories.front().id;
	}

	// First open: room for sidebar + fields (no AlwaysAutoResize).
	// Prefer a usable settings size (upgrades leftover AlwaysAutoResize crumbs).
	const ImVec2 want = uiSize(800.f, 560.f);
	ImGui::SetWindowSize(want, ImGuiCond_FirstUseEver);
	{
		const ImVec2 cur = ImGui::GetWindowSize();
		if (cur.x < uiPx(640.f) || cur.y < uiPx(420.f)) {
			ImGui::SetWindowSize(want);
		}
	}

	const float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
	const float columnHeight = ImGui::GetContentRegionAvail().y - footerHeight;

	const PrefCategory* selected = findCategory(categories, m_selectedId);
	// Resizable table = draggable divider; column width persists via imgui.ini.
	if (ImGui::BeginTable("##PrefColumns", 2, ImGuiTableFlags_Resizable)) {
		ImGui::TableSetupColumn("##Sidebar", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("##Detail", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("##PrefSidebar", ImVec2(0, columnHeight), true);
		for (const auto& cat : categories) {
			if (ImGui::Selectable(cat.label.c_str(), cat.id == m_selectedId)) {
				m_selectedId = cat.id;
			}
		}
		ImGui::EndChild();

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("##PrefDetail", ImVec2(0, columnHeight), true);
		if (selected) {
			ImGui::TextUnformatted(selected->label.c_str());
			ImGui::Separator();

			if (selected->isDrawer && mui) {
				for (const auto& drawer : mui->preferencesDrawers()) {
					if (drawer.id == selected->id && drawer.draw) {
						ImGui::PushID(drawer.id.c_str());
						drawer.draw();
						ImGui::PopID();
						break;
					}
				}
			} else {
				ImGui::PushID(selected->id.c_str());
				auto props = settings->preferenceProperties(selected->id);
				if (selected->id == "rigImGui.ui" && mui) {
					std::vector<sProp> rest;
					std::string* fontFile = nullptr;
					float* fontSize = nullptr;
					for (auto& p : props) {
						if (p.name == "Font File" && p.type == EPT_STRING) {
							fontFile = static_cast<std::string*>(p.data);
						} else if (p.name == "Font Size" && p.type == EPT_FLOAT) {
							fontSize = static_cast<float*>(p.data);
						} else {
							rest.push_back(p);
						}
					}
					bool changed = false;
					if (fontFile) {
						changed |= FontPicker::draw("uiFont", *fontFile, fontSize, mui);
					}
					if (RenderProps(nullptr, rest)) {
						changed = true;
					}
					if (ImGui::CollapsingHeader("ImGui Style")) {
						// ofxImGuiStyle parity: tweak live style, then snapshot to themes dir.
						ImGui::TextWrapped(
							"Edit the live Dear ImGui style, then Save Style to keep it "
							"(JSON under %s).",
							AppPaths::getThemesDir().c_str());
						std::string& themeFile = mui->uiPrefs().themeFile;
						char nameBuf[256];
						if (themeFile.empty()) {
							std::snprintf(nameBuf, sizeof(nameBuf), "custom.json");
						} else {
							std::snprintf(nameBuf, sizeof(nameBuf), "%s", themeFile.c_str());
						}
						if (ImGui::InputText("Style file", nameBuf, sizeof(nameBuf))) {
							themeFile = nameBuf;
							settings->markDirty();
						}
						if (ImGui::Button("Save Style")) {
							mui->saveCurrentTheme(themeFile.empty() ? "custom.json" : themeFile);
						}
						ImGui::SameLine();
						if (ImGui::Button("Load Style")) {
							mui->loadTheme(themeFile.empty() ? "custom.json" : themeFile);
						}
						ImGui::SameLine();
						if (ImGui::Button("Clear File")) {
							themeFile.clear();
							mui->setImGuiTheme(mui->getImGuiTheme());
							settings->markDirty();
						}
						ImGui::Separator();
						ImGui::ShowStyleEditor();
					}
					if (changed) {
						settings->notifyPreferenceChanged(selected->id);
					}
				} else if (RenderProps(nullptr, props)) {
					settings->notifyPreferenceChanged(selected->id);
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		ImGui::EndTable();
	}

	const bool dirty = settings->isDirty();
	if (!dirty) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Save Preferences")) {
		settings->saveToDisk();
	}
	if (!dirty) {
		ImGui::EndDisabled();
	}
	ImGui::SameLine();
	if (dirty) {
		ImGui::TextUnformatted("Unsaved changes");
	} else {
		ImGui::TextDisabled("Saved");
	}
}

} // namespace rigkit
