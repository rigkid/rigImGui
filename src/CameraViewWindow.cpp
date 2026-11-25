#include "CameraViewWindow.h"

#include "core/RigKitEngine.h"
#include "ecs/MEcs.h"
#include "rendering/TextureYOrigin.h"
#include "CCamera.h"
#include "CMesh.h"
#include "CSelection.h"
#include "CTransform.h"
#include "Mui.h"
#include "PresentImage.h"
#include "UiDpi.h"
#include "rig/create.h"
#include "rigImGui.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr int kMaxFbo = 1280;
constexpr float kDegToRad = 0.01745329252f;

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

bool sceneBounds(rigkit::MEcs& ecs, glm::vec3& bmin, glm::vec3& bmax) {
	bmin = glm::vec3(1e9f);
	bmax = glm::vec3(-1e9f);
	bool any = false;
	auto consider = [&](entt::entity e) {
		if (ecs.hasComponent<rigkit::ecs::CCamera>(e)) {
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
	return any;
}

} // namespace

namespace rigkit {

CameraViewWindow::CameraViewWindow(const std::string& title, std::string cameraName)
	: ImGuiPanel(title, ImGuiWindowFlags_NoScrollbar), m_cameraName(std::move(cameraName)) {
	setCategory("Camera");
}

void CameraViewWindow::applyPreset(Preset preset) {
	auto* engine = getEngine();
	auto* ecs = engine ? engine->getECSManager() : nullptr;
	if (!ecs) {
		return;
	}
	const auto cam = ecs->findEntity(m_cameraName);
	if (cam == entt::null || !ecs->hasComponent<ecs::COrbitDrive>(cam) ||
		!ecs->hasComponent<ecs::CCamera>(cam)) {
		return;
	}
	auto& orbit = ecs->getComponent<ecs::COrbitDrive>(cam);
	auto& optics = ecs->getComponent<ecs::CCamera>(cam);
	glm::vec3 bmin{};
	glm::vec3 bmax{};
	float span = std::max(orbit.radius, 1.f);
	if (sceneBounds(*ecs, bmin, bmax)) {
		orbit.target = 0.5f * (bmin + bmax);
		const glm::vec3 extent = bmax - bmin;
		span = std::max({extent.x, extent.y, extent.z, 1.f});
	}
	orbit.radius = span * 2.2f;
	orbit.enabled = false;

	const bool zUp = orbit.worldUp == ecs::COrbitDrive::WorldUp::Z;
	switch (preset) {
	case Preset::Top:
		orbit.yaw = 0.f;
		orbit.pitch = zUp ? (1.5707963f - 0.02f) : (1.5707963f - 0.02f);
		optics.projection = ecs::CCamera::Projection::Orthographic;
		optics.orthoHeight = span * 1.25f;
		break;
	case Preset::Front:
		orbit.yaw = 0.f;
		orbit.pitch = 0.f;
		optics.projection = ecs::CCamera::Projection::Orthographic;
		optics.orthoHeight = span * 1.25f;
		break;
	case Preset::Back:
		orbit.yaw = 3.14159265f;
		orbit.pitch = 0.f;
		optics.projection = ecs::CCamera::Projection::Orthographic;
		optics.orthoHeight = span * 1.25f;
		break;
	case Preset::Left:
		orbit.yaw = zUp ? -1.5707963f : 1.5707963f;
		orbit.pitch = 0.f;
		optics.projection = ecs::CCamera::Projection::Orthographic;
		optics.orthoHeight = span * 1.25f;
		break;
	case Preset::Right:
		orbit.yaw = zUp ? 1.5707963f : -1.5707963f;
		orbit.pitch = 0.f;
		optics.projection = ecs::CCamera::Projection::Orthographic;
		optics.orthoHeight = span * 1.25f;
		break;
	case Preset::Iso:
	default: {
		orbit.yaw = 35.f * kDegToRad;
		orbit.pitch = 25.f * kDegToRad;
		optics.projection = ecs::CCamera::Projection::Perspective;
		optics.fovYDegrees = 45.f;
		break;
	}
	}
	rig::orbitApply(*ecs, cam);
}

void CameraViewWindow::renderContents() {
	auto* engine = getEngine();
	auto* ecs = engine ? engine->getECSManager() : nullptr;
	if (!ecs) {
		ImGui::TextDisabled("No ECS");
		return;
	}
	const auto cam = ecs->findEntity(m_cameraName);
	if (cam == entt::null || !ecs->hasComponent<ecs::COrbitDrive>(cam) ||
		!ecs->hasComponent<ecs::CCamera>(cam) || !ecs->hasComponent<ecs::CTransform>(cam)) {
		ImGui::TextDisabled("Camera '%s' is gone", m_cameraName.c_str());
		return;
	}

	auto& optics = ecs->getComponent<ecs::CCamera>(cam);
	optics.active = false;

	if (ImGui::Button("Fit")) {
		glm::vec3 bmin{};
		glm::vec3 bmax{};
		if (sceneBounds(*ecs, bmin, bmax)) {
			rig::orbitFrame(*ecs, cam, bmin, bmax);
			if (optics.projection == ecs::CCamera::Projection::Orthographic) {
				const glm::vec3 extent = bmax - bmin;
				optics.orthoHeight = std::max({extent.x, extent.y, extent.z, 1.f}) * 1.25f;
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Top")) {
		applyPreset(Preset::Top);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Front")) {
		applyPreset(Preset::Front);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Iso")) {
		applyPreset(Preset::Iso);
	}

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	if (avail.x < 8.f || avail.y < 8.f) {
		return;
	}

	float dpi = 1.f;
	if (auto* mui = authorMui(*engine)) {
		dpi = mui->dpiScale();
	}
	int tw = static_cast<int>(avail.x * dpi + 0.5f);
	int th = static_cast<int>(avail.y * dpi + 0.5f);
	tw = std::clamp(tw, 8, kMaxFbo);
	th = std::clamp(th, 8, kMaxFbo);

	rig::orbitApply(*ecs, cam);
	auto& xf = ecs->getComponent<ecs::CTransform>(cam);
	xf.world = xf.localMatrix();
	if (!m_target.resize(tw, th)) {
		ImGui::TextDisabled("FBO failed");
		return;
	}
	ecs::SMeshPresent3D(*ecs, cam, tw, th, true, nullptr, 0, 0, 0, m_target.fbo());

	const ImVec2 a = ImGui::GetCursorScreenPos();
	const ImVec2 b(a.x + avail.x, a.y + avail.y);
	ImGui::InvisibleButton("##camera_view", uiHitSize(avail));
	addImagePresent(ImGui::GetWindowDrawList(), m_target.texture(), a, b, m_target.colorYOrigin());

	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();
	const ImGuiIO& io = ImGui::GetIO();
	const TextureYOrigin origin = m_target.colorYOrigin();

	rig::OrbitNavFrame frame;
	frame.mouseX = io.MousePos.x;
	frame.mouseY = io.MousePos.y;
	frame.viewW = avail.x;
	frame.viewH = avail.y;
	frame.presentOrigin = origin;
	frame.wheel = hovered ? io.MouseWheel : 0.f;
	frame.pan = active && (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
						   (ImGui::IsMouseDown(ImGuiMouseButton_Left) && io.KeyShift));
	frame.orbit = active && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !io.KeyShift && !io.KeyAlt;
	frame.blocked = !hovered && !active;

	if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyShift) {
		const float localX = io.MouseClickedPos[ImGuiMouseButton_Left].x - a.x;
		const float localY = io.MouseClickedPos[ImGuiMouseButton_Left].y - a.y;
		const float ndcX = 2.f * (localX / avail.x) - 1.f;
		const float ndcY = presentNdcYFromWidgetLocal(origin, localY, avail.y);
		const float aspect = avail.y > 1.f ? avail.x / avail.y : 1.f;
		frame.pivot = rig::orbitPickPivot(*ecs, cam, ndcX, ndcY, aspect);
		frame.havePivot = true;
		m_havePivotPress = true;
	} else if (std::fabs(frame.wheel) > 1e-4f) {
		const float localX = io.MousePos.x - a.x;
		const float localY = io.MousePos.y - a.y;
		const float ndcX = 2.f * (localX / avail.x) - 1.f;
		const float ndcY = presentNdcYFromWidgetLocal(origin, localY, avail.y);
		const float aspect = avail.y > 1.f ? avail.x / avail.y : 1.f;
		frame.pivot = rig::orbitPickPivot(*ecs, cam, ndcX, ndcY, aspect);
		frame.havePivot = true;
	} else if (frame.orbit) {
		frame.havePivot = m_havePivotPress;
		frame.pivot = m_nav.pivot;
	}
	if (!frame.orbit && !frame.pan) {
		m_havePivotPress = false;
	}

	rig::orbitNavigate(*ecs, cam, frame, m_nav);
}

entt::entity openCameraView(Mui& ui, ecs::COrbitDrive::WorldUp worldUp) {
	auto* engine = ui.getRigKitEngine();
	auto* ecs = engine ? engine->getECSManager() : nullptr;
	auto* wm = ui.getWindowManager();
	if (!ecs || !wm) {
		return entt::null;
	}
	glm::vec3 target{0.f, 0.f, 0.f};
	float radius = 5.6f;
	for (auto e : ecs->view<ecs::COrbitDrive, ecs::CCamera>()) {
		if (!ecs->getComponent<ecs::CCamera>(e).active) {
			continue;
		}
		const auto& orbit = ecs->getComponent<ecs::COrbitDrive>(e);
		target = orbit.target;
		radius = std::max(0.5f, orbit.radius);
		break;
	}
	const std::string name = uniqueViewName(*ecs, "Orbit");
	const auto cam = rig::makeOrbitCamera(*ecs, target, radius, 0.36f, false, name);
	if (cam == entt::null) {
		return entt::null;
	}
	auto& orbit = ecs->getComponent<ecs::COrbitDrive>(cam);
	orbit.worldUp = worldUp;
	orbit.enabled = false;
	if (worldUp == ecs::COrbitDrive::WorldUp::Z) {
		orbit.target = target;
		orbit.yaw = 35.f * kDegToRad;
		orbit.pitch = 25.f * kDegToRad;
	}
	rig::orbitApply(*ecs, cam);
	for (auto e : ecs->view<ecs::CCamera>()) {
		if (ecs->getComponent<ecs::CCamera>(e).active) {
			ecs->getComponent<ecs::CCamera>(cam).shade = ecs->getComponent<ecs::CCamera>(e).shade;
			break;
		}
	}
	wm->createWindow<CameraViewWindow>(name, name)->setVisible(true);
	return cam;
}

void registerCameraViewMenu(Mui& ui) {
	ui.registerViewAction(
		"Free Orbit", [&ui] { openCameraView(ui, ecs::COrbitDrive::WorldUp::Y); }, {}, "Camera");
	ui.registerViewAction(
		"Free Orbit (Host Space)", [&ui] { openCameraView(ui, ecs::COrbitDrive::WorldUp::Z); }, {},
		"Camera");
}

} // namespace rigkit
