#include "app.h"

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "core/RigKitEngine.h"
#include "core/pack/MPack.h"
#include "imgui_internal.h"
#include "packs/rigComponent/src/CLight.h"
#include "packs/rigComponent/src/CMesh.h"
#include "packs/rigComponent/src/CTransform.h"
#include "packs/rigComponent/src/rig.h"
#include "packs/rigComponent/src/rigComponent.h"
#include "packs/rigImGui/src/ImGuiStyleKit.h"
#include "packs/rigImGui/src/Mui.h"
#include "packs/rigImGui/src/PropertiesWindow.h"
#include "packs/rigImGui/src/rigImGui.h"
#include "packs/rigRender3D/src/rigRender3D.h"
#include "packs/rigSystems/src/rigSystems.h"

namespace {

entt::entity makeGroundGrid(int halfCells, float step, const rigkit::ecs::CDrawStyle& style) {
	std::vector<glm::vec3> positions;
	std::vector<uint32_t> indices;
	positions.reserve(static_cast<size_t>((halfCells * 2 + 1) * 4));
	const float extent = static_cast<float>(halfCells) * step;
	uint32_t i = 0;
	for (int c = -halfCells; c <= halfCells; ++c) {
		const float t = static_cast<float>(c) * step;
		positions.push_back({-extent, 0.f, t});
		positions.push_back({extent, 0.f, t});
		indices.push_back(i++);
		indices.push_back(i++);
		positions.push_back({t, 0.f, -extent});
		positions.push_back({t, 0.f, extent});
		indices.push_back(i++);
		indices.push_back(i++);
	}
	return rig::makeMesh(std::move(positions), std::move(indices),
						 rigkit::ecs::CMesh::Mode::Lines, style, "grid");
}

void placeBox(rigkit::MEcs& ecs, entt::entity e, glm::vec3 pos, float scale = 1.f) {
	if (e == entt::null || !ecs.hasComponent<rigkit::ecs::CTransform>(e)) {
		return;
	}
	auto& xf = ecs.getComponent<rigkit::ecs::CTransform>(e);
	xf.position = pos;
	xf.scale = {scale, scale, scale};
}

} // namespace

HostShellApp::HostShellApp() {
	window().width = 1100;
	window().height = 700;
	window().title = "rigImGui - host_shell";
	settings().appName = "host_shell";
}

void HostShellApp::WidgetsWindow::renderContents() {
	ImGui::TextWrapped(
		"Host shell for rigImGui. Open View > Theme for accents; "
		"Preferences holds the full Style Editor.");
	ImGui::Separator();
	ImGui::Checkbox("Demo checkbox", &m_checked);
	ImGui::SliderFloat("Slider", &m_slider, 0.f, 1.f);
	const char* items[] = {"Alpha", "Beta", "Gamma"};
	ImGui::Combo("Combo", &m_combo, items, 3);
	ImGui::ColorEdit4("Color", m_color, ImGuiColorEditFlags_NoInputs);
	if (ImGui::Button("Ping")) {
		if (auto* mui = dynamic_cast<rigkit::Mui*>(getEngine()->getUiManager())) {
			mui->showNotification("Widgets: Ping");
		}
	}
	ImGui::SameLine();
	ImGui::BeginDisabled();
	ImGui::Button("Disabled");
	ImGui::EndDisabled();

	ImGui::Separator();
	ImGui::TextUnformatted("List");
	if (ImGui::BeginChild("##list", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
		for (int i = 0; i < 12; ++i) {
			ImGui::Selectable(("Row " + std::to_string(i)).c_str(), i == 3);
		}
	}
	ImGui::EndChild();
}

void HostShellApp::setup() {
	spdlog::info("host_shell - Dark author shell with 3D bed");
	m_engine->setClearColor(0.10f, 0.11f, 0.13f, 1.0f);

	auto* packs = m_engine->getPackManager();
	if (!packs) {
		return;
	}
	packs->registerPack<rigkit::rigComponent>();
	packs->registerPack<rigkit::rigSystems>();
	packs->registerPack<rigkit::rigRender3D>();
	packs->registerPack<rigkit::rigImGui>();
	packs->initAll();
	packs->setupAll();

	auto* ecs = m_engine->getECSManager();
	if (!ecs) {
		return;
	}

	// Camera looks at the origin; boxes + line grid fill the central dock.
	const auto cam = rig::makeCamera({5.5f, 4.0f, 6.5f}, true, "camera");
	if (cam != entt::null) {
		rig::lookAt(ecs->getComponent<rigkit::ecs::CTransform>(cam), {5.5f, 4.0f, 6.5f},
					{0.f, 0.5f, 0.f});
	}

	// Directional light shines along local -Z — aim it at the stage.
	const auto light =
		rig::makeLight({0.f, 0.f, 0.f}, rigkit::ecs::CLight::Type::Directional, "key-light");
	if (light != entt::null) {
		rig::lookAt(ecs->getComponent<rigkit::ecs::CTransform>(light), {4.f, 7.f, 5.f},
					{0.f, 0.f, 0.f});
		auto& L = ecs->getComponent<rigkit::ecs::CLight>(light);
		L.intensity = 1.35f;
		L.ambient = 0.22f;
		L.banded = false;
		L.colorR = 1.0f;
		L.colorG = 0.97f;
		L.colorB = 0.92f;
	}

	makeGroundGrid(6, 1.f, rig::stroke(0.45f, 0.48f, 0.52f, 0.85f, 1.f));

	const auto box =
		rig::makeMeshBox(1.2f, rig::fill(0.25f, 0.55f, 0.90f), "demo-box");
	placeBox(*ecs, box, {-1.4f, 0.6f, 0.2f}, 1.f);
	placeBox(*ecs, rig::makeMeshBox(0.9f, rig::fill(0.95f, 0.55f, 0.25f), "demo-block"),
			 {1.2f, 0.45f, -0.6f}, 1.f);
	placeBox(*ecs, rig::makeMeshBox(0.7f, rig::fill(0.35f, 0.78f, 0.55f), "demo-cube"),
			 {0.2f, 0.35f, 1.4f}, 1.f);

	if (auto* mui = dynamic_cast<rigkit::Mui*>(m_engine->getUiManager())) {
		mui->setImGuiTheme(rigkit::ImGuiTheme::Dark);
		mui->uiPrefs().showStatusBar = true;
		mui->uiPrefs().fontSize = 16.f;
		mui->reloadFonts();
		mui->setDockPassthroughCentral(true);

		mui->addHostPanel(rigkit::HostPanel::Scene);
		mui->addHostPanel(rigkit::HostPanel::Layers);
		mui->addHostPanel(rigkit::HostPanel::Windows);
		mui->addHostPanel(rigkit::HostPanel::Properties);
		mui->addHostPanel(rigkit::HostPanel::Log);
		mui->addHostPanel(rigkit::HostPanel::Theme);

		if (auto* wm = mui->getWindowManager()) {
			wm->hideWindow("Theme");
			auto widgets = wm->createWindow<WidgetsWindow>();
			if (widgets) {
				widgets->setVisible(true);
			}
		}

		mui->setDockLayoutBuilder([mui](ImGuiID dockspaceId) {
			ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
			if (!node) {
				return;
			}
			if (node->IsSplitNode()) {
				mui->setDockLayoutBuilder(nullptr);
				return;
			}
			const ImGuiViewport* vp = ImGui::GetMainViewport();
			ImGui::DockBuilderRemoveNode(dockspaceId);
			ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspaceId, vp->WorkSize);

			ImGuiID left = 0, center = 0, right = 0, bottom = 0;
			ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.20f, &left, &center);
			ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.30f, &right, &center);
			ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, &bottom, &center);

			ImGui::DockBuilderDockWindow("Scene", left);
			ImGui::DockBuilderDockWindow("Layers", left);
			ImGui::DockBuilderDockWindow("Windows", left);
			ImGui::DockBuilderDockWindow("Widgets", left);
			ImGui::DockBuilderDockWindow("Properties", right);
			ImGui::DockBuilderDockWindow("Log", bottom);
			ImGui::DockBuilderFinish(dockspaceId);
			mui->setDockLayoutBuilder(nullptr);
		});

		if (auto* wm = mui->getWindowManager()) {
			wm->showWindow("Scene");
			wm->showWindow("Layers");
			wm->showWindow("Windows");
			wm->showWindow("Widgets");
			wm->showWindow("Properties");
			wm->showWindow("Log");
			if (auto props = wm->getWindow<rigkit::PropertiesWindow>("Properties")) {
				props->setSelectedEntity(static_cast<uint32_t>(box));
			}
		}

		spdlog::info("host_shell ready - 3D bed with grid + docked author panels");
		spdlog::warn("Try View > Theme, then Preferences for the Style Editor");
	}
}
