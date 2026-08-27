#include "app.h"

#include "core/pack/MPack.h"
#include "core/RigKitEngine.h"
#include "packs/rigImGui/src/Mui.h"
#include "packs/rigImGui/src/rigImGui.h"

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <spdlog/spdlog.h>

ImGuizmoApp::ImGuizmoApp() {
	window().width = 880;
	window().height = 560;
	window().title = "rigImGui - example_ImGuizmo";
	settings().appName = "example_ImGuizmo";
}

void ImGuizmoApp::setup() {
	spdlog::info("example_ImGuizmo - translate gizmo via Mui::setGizmoDrawer");
	m_engine->setClearColor(0.08f, 0.09f, 0.11f, 1.0f);

	auto* packs = m_engine->getPackManager();
	if (!packs) {
		return;
	}
	packs->registerPack<rigkit::rigImGui>();
	packs->initAll();
	packs->setupAll();

	if (auto* mui = dynamic_cast<rigkit::Mui*>(m_engine->getUiManager())) {
		mui->uiPrefs().showStatusBar = false;
		mui->setWindowVisibilityAll(false);
		mui->setDockPassthroughCentral(true);
		mui->setGizmoOp(rigkit::IMui::GizmoOp::Translate);
		// Runs inside Mui::render() after NewFrame - never from IApp::draw().
		mui->setGizmoDrawer([this](float x, float y, float w, float h, rigkit::IMui::GizmoOp) {
			if (w < 1.f || h < 1.f) {
				return;
			}
			ImGuizmo::BeginFrame();
			ImGuizmo::SetOrthographic(false);
			ImDrawList* dl = ImGui::GetBackgroundDrawList();
			ImGuizmo::SetDrawlist(dl);
			ImGuizmo::SetRect(x, y, w, h);
			dl->PushClipRect(ImVec2(x, y), ImVec2(x + w, y + h), true);

			const glm::mat4 view =
				glm::lookAt(glm::vec3(4.f, 3.f, 5.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
			// ImGuizmo + OpenGL: flip Y on the projection (otherwise grid/gizmo draw upside down).
			glm::mat4 proj = glm::perspective(glm::radians(45.f), w / h, 0.1f, 100.f);
			proj[1][1] *= -1.f;

			ImGuizmo::DrawGrid(glm::value_ptr(view), glm::value_ptr(proj), m_matrix, 8.f);
			ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), ImGuizmo::TRANSLATE,
								 ImGuizmo::LOCAL, m_matrix);
			dl->PopClipRect();

			ImGui::SetNextWindowPos(ImVec2(x + 16.f, y + 16.f), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.65f);
			if (ImGui::Begin("ImGuizmo", nullptr,
							 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking |
								 ImGuiWindowFlags_NoCollapse)) {
				ImGui::TextUnformatted("Drag the gizmo to move the object.");
				float t[3], r[3], s[3];
				ImGuizmo::DecomposeMatrixToComponents(m_matrix, t, r, s);
				ImGui::Text("T %.2f %.2f %.2f", t[0], t[1], t[2]);
			}
			ImGui::End();
		});
	}
}
