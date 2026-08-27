#include "app.h"

#include "core/pack/MPack.h"
#include "core/RigKitEngine.h"
#include "packs/rigImGui/src/Mui.h"
#include "packs/rigImGui/src/rigImGui.h"
#include "imgui_internal.h"

#include <spdlog/spdlog.h>

SampleMenubarApp::SampleMenubarApp() {
	window().width = 880;
	window().height = 560;
	window().title = "rigImGui — sample_menubar";
	settings().appName = "sample_menubar";
}

void SampleMenubarApp::setup() {
	spdlog::info("sample_menubar — host menu bar + Log");
	m_engine->setClearColor(0.12f, 0.12f, 0.14f, 1.0f);

	auto* packs = m_engine->getPackManager();
	if (!packs) {
		return;
	}
	packs->registerPack<rigkit::rigImGui>();
	packs->initAll();
	packs->setupAll();

	if (auto* mui = dynamic_cast<rigkit::Mui*>(m_engine->getUiManager())) {
		mui->uiPrefs().showStatusBar = false;
		mui->addHostPanel(rigkit::HostPanel::Log);
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
			ImGui::DockBuilderDockWindow("Log", dockspaceId);
			ImGui::DockBuilderFinish(dockspaceId);
			mui->setDockLayoutBuilder(nullptr);
		});
		if (auto* wm = mui->getWindowManager()) {
			wm->showWindow("Log");
		}
	}
}
