#include "OrthoViewWindow.h"

#include "CCamera.h"
#include "CMesh.h"
#include "COrbitDrive.h"
#include "CSelection.h"
#include "CTransform.h"
#include "Mui.h"
#include "UiDpi.h"
#include "core/RigKitEngine.h"
#include "ecs/MEcs.h"
#include "rig/create.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace {

constexpr int kMaxFbo = 1280;

const char* faceLabel(rigkit::ecs::COrthoView::Face face) {
	switch (face) {
	case rigkit::ecs::COrthoView::Face::Bottom:
		return "Bottom";
	case rigkit::ecs::COrthoView::Face::Left:
		return "Left";
	case rigkit::ecs::COrthoView::Face::Right:
		return "Right";
	case rigkit::ecs::COrthoView::Face::Top:
	default:
		return "Top";
	}
}

std::string uniqueViewName(rigkit::MEcs& ecs, const char* base) {
	if (ecs.findEntity(base) == entt::null) {
		return base;
	}
	for (int i = 2; i < 1000; ++i) {
		const std::string n = std::string(base) + "-" + std::to_string(i);
		if (ecs.findEntity(n) == entt::null) {
			return n;
		}
	}
	return std::string(base) + "-x";
}

bool meshWorldBounds(rigkit::MEcs& ecs, entt::entity e, glm::vec3& bmin, glm::vec3& bmax) {
	if (!ecs.hasComponent<rigkit::ecs::CMesh>(e) || !ecs.hasComponent<rigkit::ecs::CTransform>(e)) {
		return false;
	}
	const auto& mesh = ecs.getComponent<rigkit::ecs::CMesh>(e);
	const auto& xf = ecs.getComponent<rigkit::ecs::CTransform>(e);
	if (mesh.positions.empty()) {
		return false;
	}
	for (const auto& p : mesh.positions) {
		const glm::vec3 w = glm::vec3(xf.world * glm::vec4(p, 1.f));
		bmin = glm::min(bmin, w);
		bmax = glm::max(bmax, w);
	}
	return true;
}

void frameOrthoView(rigkit::MEcs& ecs, entt::entity cam, float aspect) {
	if (!ecs.hasComponent<rigkit::ecs::COrthoView>(cam) ||
		!ecs.hasComponent<rigkit::ecs::CCamera>(cam)) {
		return;
	}
	glm::vec3 bmin(1e9f);
	glm::vec3 bmax(-1e9f);
	bool any = false;
	auto consider = [&](entt::entity e) {
		if (ecs.hasComponent<rigkit::ecs::CCamera>(e) ||
			ecs.hasComponent<rigkit::ecs::COrthoView>(e)) {
			return;
		}
		if (meshWorldBounds(ecs, e, bmin, bmax)) {
			any = true;
		}
	};
	for (auto e : ecs.view<rigkit::ecs::CSelection>()) {
		const auto& sel = ecs.getComponent<rigkit::ecs::CSelection>(e);
		if (sel.isSelected || sel.isMultiSelected) {
			consider(e);
		}
	}
	if (!any) {
		for (auto e : ecs.view<rigkit::ecs::CMesh, rigkit::ecs::CTransform>()) {
			consider(e);
		}
	}
	if (!any) {
		return;
	}
	auto& view = ecs.getComponent<rigkit::ecs::COrthoView>(cam);
	view.target = 0.5f * (bmin + bmax);
	const glm::vec3 size = bmax - bmin;
	float spanA = size.x;
	float spanB = size.z;
	if (view.face == rigkit::ecs::COrthoView::Face::Left ||
		view.face == rigkit::ecs::COrthoView::Face::Right) {
		spanA = size.z;
		spanB = size.y;
	}
	const float a = aspect > 1e-3f ? aspect : 1.f;
	ecs.getComponent<rigkit::ecs::CCamera>(cam).orthoHeight =
		std::max(spanB, spanA / a) * 1.15f + 0.05f;
}

} // namespace

namespace rigkit {

OrthoViewWindow::OrthoViewWindow(const std::string& title, std::string cameraName)
	: IWindow(title, ImGuiWindowFlags_NoScrollbar), m_cameraName(std::move(cameraName)) {
	setCategory("Camera");
}

void OrthoViewWindow::renderContents() {
	auto* engine = getEngine();
	auto* ecs = engine ? engine->getECSManager() : nullptr;
	if (!ecs) {
		ImGui::TextDisabled("No ECS");
		return;
	}
	const auto cam = ecs->findEntity(m_cameraName);
	if (cam == entt::null || !ecs->hasComponent<ecs::COrthoView>(cam) ||
		!ecs->hasComponent<ecs::CCamera>(cam) || !ecs->hasComponent<ecs::CTransform>(cam)) {
		ImGui::TextDisabled("Camera '%s' is gone", m_cameraName.c_str());
		return;
	}

	auto& view = ecs->getComponent<ecs::COrthoView>(cam);
	auto& optics = ecs->getComponent<ecs::CCamera>(cam);
	auto& xf = ecs->getComponent<ecs::CTransform>(cam);
	optics.active = false;
	optics.projection = ecs::CCamera::Projection::Orthographic;

	if (ImGui::Button("Fit")) {
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		frameOrthoView(*ecs, cam, avail.y > 1.f ? avail.x / avail.y : 1.f);
	}
	ImGui::SameLine();
	ImGui::TextUnformatted(faceLabel(view.face));

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	if (avail.x < 8.f || avail.y < 8.f) {
		return;
	}

	float dpi = 1.f;
	if (auto* mui = dynamic_cast<Mui*>(engine->getUiManager())) {
		dpi = mui->dpiScale();
	}
	int tw = static_cast<int>(avail.x * dpi + 0.5f);
	int th = static_cast<int>(avail.y * dpi + 0.5f);
	tw = std::clamp(tw, 8, kMaxFbo);
	th = std::clamp(th, 8, kMaxFbo);

	rig::poseOrthoView(xf, view, std::max(2.f, optics.orthoHeight));
	xf.world = xf.localMatrix();
	if (!m_target.resize(tw, th)) {
		ImGui::TextDisabled("FBO failed");
		return;
	}
	ecs::SMeshPresent3D(*ecs, cam, tw, th, true, nullptr, 0, 0, 0, m_target.fbo());

	const ImVec2 a = ImGui::GetCursorScreenPos();
	const ImVec2 b(a.x + avail.x, a.y + avail.y);
	ImGui::InvisibleButton("##ortho_view", uiHitSize(avail));
	ImGui::GetWindowDrawList()->AddImage(
		static_cast<ImTextureID>(static_cast<intptr_t>(m_target.texture())), a, b, ImVec2(0.f, 1.f),
		ImVec2(1.f, 0.f));

	const bool hovered = ImGui::IsItemHovered();
	const ImGuiIO& io = ImGui::GetIO();
	if (hovered && io.MouseWheel != 0.f) {
		optics.orthoHeight = std::max(0.05f, optics.orthoHeight * std::pow(0.9f, io.MouseWheel));
	}
	if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
					(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyAlt))) {
		m_panning = true;
	}
	if (m_panning) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
			(ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyAlt)) {
			const float s = optics.orthoHeight / std::max(1.f, avail.y);
			const glm::vec3 right{xf.world[0].x, xf.world[0].y, xf.world[0].z};
			const glm::vec3 up{xf.world[1].x, xf.world[1].y, xf.world[1].z};
			view.target -= right * (io.MouseDelta.x * s);
			view.target += up * (io.MouseDelta.y * s);
		} else {
			m_panning = false;
		}
	}
}

entt::entity openOrthoView(Mui& ui, ecs::COrthoView::Face face) {
	auto* engine = ui.getRigKitEngine();
	auto* ecs = engine ? engine->getECSManager() : nullptr;
	auto* wm = ui.getWindowManager();
	if (!ecs || !wm) {
		return entt::null;
	}
	const char* base = faceLabel(face);
	glm::vec3 target{0.f, 0.f, 0.f};
	float height = 4.f;
	for (auto e : ecs->view<ecs::COrbitDrive, ecs::CCamera>()) {
		if (!ecs->getComponent<ecs::CCamera>(e).active) {
			continue;
		}
		const auto& orbit = ecs->getComponent<ecs::COrbitDrive>(e);
		target = orbit.target;
		height = std::max(0.4f, orbit.radius * 0.7f);
		break;
	}
	const std::string name = uniqueViewName(*ecs, base);
	const auto cam = rig::makeOrthoCamera(*ecs, face, target, height, name);
	if (cam == entt::null) {
		return entt::null;
	}
	for (auto e : ecs->view<ecs::CCamera>()) {
		if (ecs->getComponent<ecs::CCamera>(e).active) {
			ecs->getComponent<ecs::CCamera>(cam).shade = ecs->getComponent<ecs::CCamera>(e).shade;
			break;
		}
	}
	wm->createWindow<OrthoViewWindow>(name, name)->setVisible(true);
	return cam;
}

void registerOrthoViewMenu(Mui& ui) {
	ui.registerViewSubmenu("Camera", [&ui] {
		if (ImGui::MenuItem("Top")) {
			openOrthoView(ui, ecs::COrthoView::Face::Top);
		}
		if (ImGui::MenuItem("Bottom")) {
			openOrthoView(ui, ecs::COrthoView::Face::Bottom);
		}
		if (ImGui::MenuItem("Left")) {
			openOrthoView(ui, ecs::COrthoView::Face::Left);
		}
		if (ImGui::MenuItem("Right")) {
			openOrthoView(ui, ecs::COrthoView::Face::Right);
		}
	});
}

} // namespace rigkit
