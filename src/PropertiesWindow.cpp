#include "PropertiesWindow.h"
#include <imgui.h>
#include "CCamera.h"
#include "CCode.h"
#include "CDriveHint.h"
#include "CTransform.h"
#include "PropEditors.h"
#include "SceneDragPayload.h"
#include "UiDpi.h"
#include "core/IMui.h"
#include "core/RigKitEngine.h"
#include "core/util/MSettings.h"
#include "core/util/UndoStack.h"
#include "ecs/MEcs.h"
#include "ecs/PropertyReflection.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace rigkit {

namespace {
constexpr float kEntityListMinHeight = 40.0f;
constexpr float kEntityListGrip = 6.0f;
constexpr const char* kEntityListHeightKey = "properties.entityListHeight";
constexpr const char* kEntityListOpenKey = "properties.entityListOpen";
constexpr const char* kCodeEditHeightKey = "properties.codeEditHeight";
constexpr int kCodePreviewMaxChars = 4000;
constexpr float kCodeEditMinHeight = 80.f;
constexpr float kCodeEditDefaultHeight = 220.f;
constexpr float kCodeEditGrip = 6.0f;

int inputTextResizeCallback(ImGuiInputTextCallbackData* data) {
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		auto* str = static_cast<std::string*>(data->UserData);
		str->resize(static_cast<size_t>(data->BufTextLen));
		data->Buf = str->data();
	}
	return 0;
}

bool inputTextMultilineString(const char* label, std::string& str, const ImVec2& size,
							  bool readOnly) {
	// ImGui writes through data() and grows via CallbackResize — keep spare capacity.
	if (str.capacity() <= str.size()) {
		str.reserve(str.size() + 256);
	}
	ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize;
	if (readOnly) {
		flags |= ImGuiInputTextFlags_ReadOnly;
	}
	return ImGui::InputTextMultiline(label, str.data(), str.capacity() + 1, size, flags,
									 inputTextResizeCallback, &str);
}

int countLines(const std::string& text) {
	if (text.empty()) {
		return 1;
	}
	int n = 1;
	for (char c : text) {
		if (c == '\n') {
			++n;
		}
	}
	return n;
}

/**
 * @brief Undo/redo apply for one inspector property.
 * @details Re-resolves the field through registered properties each time —
 * records hold no raw field pointers, so component storage may move freely.
 */
void applyPropValue(MEcs& ecs, uint32_t entityId, const std::string& component, uint32_t propId,
					const PropValue& value) {
	const auto entity = static_cast<entt::entity>(entityId);
	if (!ecs.registry().valid(entity)) {
		return;
	}
	for (const auto& info : ecs.componentTypes()) {
		if (info.name != component) {
			continue;
		}
		if (!ecs.hasRegisteredComponent(info, entity)) {
			return;
		}
		auto props = ecs.registeredProperties(info, entity);
		for (auto& p : props) {
			if (p.id == propId) {
				writePropValue(p, value);
				break;
			}
		}
		// Same follow-up the live inspector does after a Transform edit.
		if (component == "Transform" && ecs.hasComponent<ecs::CTransform>(entity)) {
			ecs.getComponent<ecs::CTransform>(entity).syncRotationFromEuler();
		}
		return;
	}
}
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

void PropertiesWindow::setOnOpenCodeEditor(OpenCodeEditorFn callback) {
	m_onOpenCodeEditor = std::move(callback);
}

void PropertiesWindow::setCodeLightEditDraw(CodeLightEditDrawFn callback) {
	m_codeLightEditDraw = std::move(callback);
}

void PropertiesWindow::openInCodeEditor(uint32_t entity) {
	if (m_onOpenCodeEditor) {
		m_onOpenCodeEditor(entity);
	}
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
						   uiHitSize(ImVec2(ImGui::GetContentRegionAvail().x, kEntityListGrip)));
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

void PropertiesWindow::loadCodeEditHeight() {
	if (m_codeEditHeightLoaded) {
		return;
	}
	m_codeEditHeightLoaded = true;
	m_codeEditHeight = kCodeEditDefaultHeight;
	MSettings* store = settings();
	if (!store) {
		return;
	}
	const json height = store->getValue(kCodeEditHeightKey);
	if (height.is_number()) {
		m_codeEditHeight = height.get<float>();
	}
}

void PropertiesWindow::saveCodeEditHeight() {
	if (MSettings* store = settings()) {
		store->setValue(kCodeEditHeightKey, m_codeEditHeight);
		store->saveToDisk();
	}
}

void PropertiesWindow::renderCodeEditHeightGrip() {
	ImGui::InvisibleButton("CodeEditHeightGrip",
						   uiHitSize(ImVec2(ImGui::GetContentRegionAvail().x, kCodeEditGrip)));
	const bool active = ImGui::IsItemActive();
	const bool hovered = ImGui::IsItemHovered();
	if (active) {
		m_codeEditHeight += ImGui::GetIO().MouseDelta.y;
	}
	if (active || hovered) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	}
	if (ImGui::IsItemDeactivated()) {
		saveCodeEditHeight();
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

void PropertiesWindow::renderCodeEditSection(MEcs& ecs, entt::entity entity) {
	if (!ecs.hasComponent<ecs::CCode>(entity)) {
		return;
	}

	auto& code = ecs.getComponent<ecs::CCode>(entity);
	const uint32_t entityId = static_cast<uint32_t>(entity);
	const bool readOnly = code.readOnly;

	ImGui::TextDisabled("%s · %d lines%s",
						code.language.empty() ? "plain" : code.language.c_str(),
						countLines(code.text), code.dirty ? " · dirty" : "");
	if (m_onOpenCodeEditor) {
		if (ImGui::Button("Open in Code Editor")) {
			m_onOpenCodeEditor(entityId);
		}
	}

	loadCodeEditHeight();
	const float maxHeight =
		std::max(kCodeEditMinHeight, ImGui::GetContentRegionAvail().y - kCodeEditGrip - 8.f);
	m_codeEditHeight = std::clamp(m_codeEditHeight, kCodeEditMinHeight, maxHeight);

	// Collapsed = skip widget work (preview string / TextEditor).
	if (ImGui::CollapsingHeader("Preview")) {
		std::string preview = code.text;
		const bool truncated = static_cast<int>(preview.size()) > kCodePreviewMaxChars;
		if (truncated) {
			preview.resize(static_cast<size_t>(kCodePreviewMaxChars));
			preview += "\n…";
		}
		ImGui::BeginChild("code_preview", ImVec2(-1.f, m_codeEditHeight), true);
		ImGui::TextUnformatted(preview.empty() ? "(empty)" : preview.c_str());
		ImGui::EndChild();
		renderCodeEditHeightGrip();
		if (truncated) {
			ImGui::TextDisabled("Preview truncated — open Edit or the Code Editor for the full buffer");
		}
	}

	if (!readOnly && ImGui::CollapsingHeader("Edit")) {
		bool changed = false;
		if (m_codeLightEditDraw) {
			changed = m_codeLightEditDraw(entityId, code.text, code.language, m_codeEditHeight,
										  false);
		} else {
			ImGui::PushID("code_light_edit");
			changed = inputTextMultilineString("##body", code.text, ImVec2(-1.f, m_codeEditHeight),
											   false);
			ImGui::PopID();
		}
		if (changed) {
			code.dirty = true;
			if (m_onPropertyChanged) {
				m_onPropertyChanged(entityId, "Code", {});
			}
		}
		renderCodeEditHeightGrip();
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

	// When the host bound an undo stack, committed edits become undo records.
	UndoStack* undoStack = nullptr;
	if (auto* ui = getEngine()->getUiManager()) {
		undoStack = ui->undoStack();
	}
	auto makeCommit = [&](std::string component) -> PropCommitFn {
		if (!undoStack) {
			return {};
		}
		return [ecs, entityId, undoStack, component = std::move(component),
				cb = m_onPropertyChanged](const PropCommit& c) {
			undoStack->pushSnapshot(component + " " + c.propName, c.before, c.after,
									[ecs, entityId, component, propId = c.propId,
									 cb](const PropValue& v) {
										applyPropValue(*ecs, entityId, component, propId, v);
										if (cb) {
											cb(entityId, component, {});
										}
									});
		};
	};

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
			RenderProps(nullptr, rest, entityId, makeCommit("Camera"));
			continue;
		}

		auto props = ecs->registeredProperties(info, entity);
		const bool changed = RenderProps(info.name.c_str(), props, entityId, makeCommit(info.name));
		// Inspector edits euler floats; keep authoritative quat in sync.
		if (changed && info.name == "Transform" && ecs->hasComponent<ecs::CTransform>(entity)) {
			ecs->getComponent<ecs::CTransform>(entity).syncRotationFromEuler();
		}
		if (changed && m_onPropertyChanged) {
			m_onPropertyChanged(entityId, info.name, {});
		}
	}

	renderCodeEditSection(*ecs, entity);

	for (const auto& drawer : m_extraDrawers) {
		if (drawer) {
			drawer(*ecs, entity);
		}
	}
}

} // namespace rigkit
