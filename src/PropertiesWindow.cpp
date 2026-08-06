#include "PropertiesWindow.h"
#include <imgui.h>
#include "CCamera.h"
#include "CDriveHint.h"
#include "CTransform.h"
#include "PropEditors.h"
#include "SceneDragPayload.h"
#include "core/RigKitEngine.h"
#include "core/util/MSettings.h"
#include "ecs/MEcs.h"
#include "ecs/PropertyReflection.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace rigkit {

namespace {
constexpr float kEntityListMinHeight = 40.0f;
constexpr float kEntityListGrip = 6.0f;
constexpr const char* kEntityListHeightKey = "properties.entityListHeight";
constexpr const char* kEntityListOpenKey = "properties.entityListOpen";
} // namespace

PropertiesWindow::PropertiesWindow(const std::string& title, ImGuiWindowFlags flags)
	: IWindow(title, flags) {}

void PropertiesWindow::setSelectedEntity(uint32_t entity) {
	m_selectedEntity = entity;
	if (m_onEntitySelected) {
		m_onEntitySelected(entity);
	}
}

uint32_t PropertiesWindow::getSelectedEntity() const {
	return m_selectedEntity;
}

void PropertiesWindow::setOnEntitySelected(std::function<void(uint32_t)> callback) {
	m_onEntitySelected = callback;
}

void PropertiesWindow::setOnPropertyChanged(
	std::function<void(uint32_t, const std::string&, const std::string&)> callback) {
	m_onPropertyChanged = callback;
}

void PropertiesWindow::addExtraDrawer(ExtraDrawer drawer) {
	if (drawer) {
		m_extraDrawers.push_back(std::move(drawer));
	}
}

void PropertiesWindow::renderContents() {
	renderEntityList();
	if (m_selectedEntity != 0 && getEngine()) {
		renderAllComponentProperties();
	}
}

void PropertiesWindow::renderEntityList() {
	if (!getEngine())
		return;

	auto* ecs = getEngine()->getECSManager();
	if (!ecs)
		return;

	loadEntityListState();

	ImGui::SetNextItemOpen(m_entityListOpen, ImGuiCond_Once);
	const bool open = ImGui::CollapsingHeader("Entities");
	if (open != m_entityListOpen) {
		m_entityListOpen = open;
		saveEntityListState();
	}
	if (!open) {
		return;
	}

	// Leave room for the grip plus a few property rows.
	const float maxHeight = std::max(kEntityListMinHeight,
									 ImGui::GetContentRegionAvail().y - kEntityListGrip - 60.0f);
	m_entityListHeight = std::clamp(m_entityListHeight, kEntityListMinHeight, maxHeight);

	ImGui::BeginChild("EntityList", ImVec2(0, m_entityListHeight), true);
	for (auto entity : ecs->getAllEntities()) {
		const std::string name = ecs->entityName(entity);
		char label[256];
		snprintf(label, sizeof(label), "%s###ent_%u",
				 name.empty() ? "(unnamed)" : name.c_str(),
				 (unsigned int)entity);
		bool isSelected = (entity == static_cast<entt::entity>(m_selectedEntity));
		if (ImGui::Selectable(label, isSelected)) {
			setSelectedEntity(static_cast<uint32_t>(entity));
		}
	}
	ImGui::EndChild();

	renderEntityListGrip();
}

void PropertiesWindow::renderEntityListGrip() {
	ImGui::InvisibleButton("EntityListGrip",
						   ImVec2(ImGui::GetContentRegionAvail().x, kEntityListGrip));
	const bool active = ImGui::IsItemActive();
	const bool hovered = ImGui::IsItemHovered();
	if (active) {
		m_entityListHeight += ImGui::GetIO().MouseDelta.y;
	}
	if (active || hovered) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}
	if (ImGui::IsItemDeactivated()) {
		saveEntityListState();
	}

	ImGuiCol style = ImGuiCol_Separator;
	if (active) {
		style = ImGuiCol_SeparatorActive;
	} else if (hovered) {
		style = ImGuiCol_SeparatorHovered;
	}

	const ImVec2 gripMin = ImGui::GetItemRectMin();
	const ImVec2 gripMax = ImGui::GetItemRectMax();
	const float y = (gripMin.y + gripMax.y) * 0.5f;
	ImGui::GetWindowDrawList()->AddLine(ImVec2(gripMin.x, y), ImVec2(gripMax.x, y),
									   ImGui::GetColorU32(style));
}

MSettings* PropertiesWindow::settings() const {
	return getEngine() ? getEngine()->getSettingsManager() : nullptr;
}

void PropertiesWindow::loadEntityListState() {
	if (m_entityListStateLoaded) {
		return;
	}
	m_entityListStateLoaded = true;

	MSettings* store = settings();
	if (!store) {
		return;
	}
	const json height = store->getValue(kEntityListHeightKey);
	if (height.is_number()) {
		m_entityListHeight = height.get<float>();
	}
	const json openState = store->getValue(kEntityListOpenKey);
	if (openState.is_boolean()) {
		m_entityListOpen = openState.get<bool>();
	}
}

void PropertiesWindow::saveEntityListState() {
	if (MSettings* store = settings()) {
		store->setValue(kEntityListHeightKey, m_entityListHeight);
		store->setValue(kEntityListOpenKey, m_entityListOpen);
		// Only on drag release / header toggle, so the write stays off the hot path.
		store->saveToDisk();
	}
}

void PropertiesWindow::renderAllComponentProperties() {
	if (!getEngine())
		return;

	auto* ecs = getEngine()->getECSManager();
	if (!ecs)
		return;

	entt::entity entity = static_cast<entt::entity>(m_selectedEntity);
	const uint32_t entityId = m_selectedEntity;

	if (ecs->hasComponent<ecs::CDriveHint>(entity)) {
		const auto& hint = ecs->getComponent<ecs::CDriveHint>(entity);
		ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.f, 1.f), "Graph drive");
		ImGui::TextWrapped("%s", hint.label.c_str());
		ImGui::TextDisabled("Drag a property row into the Node Editor to make a Ref.");
		ImGui::Separator();
	}

	for (const auto& info : ecs->componentTypes()) {
		if (!ecs->hasRegisteredComponent(info, entity)) {
			continue;
		}
		if (info.name == "DriveHint") {
			continue; // shown above
		}

		if (info.name == "Camera" && ecs->hasComponent<ecs::CCamera>(entity)) {
			if (!ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
				continue;
			}
			auto& cam = ecs->getComponent<ecs::CCamera>(entity);
			ImGui::Checkbox("Active", &cam.active);
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				RigScenePropPayload payload;
				payload.entity = entityId;
				payload.propType = EPT_BOOL;
				std::snprintf(payload.name, sizeof(payload.name), "Active");
				ImGui::SetDragDropPayload(kRigScenePropPayload, &payload, sizeof(payload));
				ImGui::Text("Ref → Active");
				ImGui::EndDragDropSource();
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset")) {
				cam.resetToDefaults();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Restore FOV / clips / projection (keeps Active + present size)");
			}
			auto props = cam.GetProperties();
			std::vector<sProp> rest;
			rest.reserve(props.size());
			for (auto& p : props) {
				if (p.name == "Active") {
					continue;
				}
				rest.push_back(p);
			}
			RenderProps(nullptr, rest, entityId);
			continue;
		}

		auto props = ecs->registeredProperties(info, entity);
		const bool changed = RenderProps(info.name.c_str(), props, entityId);
		// Inspector edits euler floats; keep authoritative quat in sync.
		if (changed && info.name == "Transform" && ecs->hasComponent<ecs::CTransform>(entity)) {
			ecs->getComponent<ecs::CTransform>(entity).syncRotationFromEuler();
		}
		if (changed && m_onPropertyChanged) {
			m_onPropertyChanged(entityId, info.name, {});
		}
	}

	for (const auto& drawer : m_extraDrawers) {
		if (drawer) {
			drawer(*ecs, entity);
		}
	}
}

} // namespace rigkit
