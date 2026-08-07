#include "PropEditors.h"

#include "SceneDragPayload.h"

#include <cstdio>
#include <cstring>
#include <glm/glm.hpp>
#include <imgui.h>
#include <string>

namespace rigkit {
namespace {

bool ImGuiInputTextStdString(const char* label, std::string& str) {
	char buffer[256];
	strncpy(buffer, str.c_str(), sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = '\0';
	bool changed = ImGui::InputText(label, buffer, sizeof(buffer));
	if (changed) {
		str = buffer;
	}
	return changed;
}

/// Pre-edit value of the widget being edited right now. One slot is enough:
/// ImGui has a single active item, and identity is re-checked on commit.
struct ActivePropEdit {
	bool valid = false;
	ImGuiID itemId = 0;
	const void* field = nullptr;
	PropValue before;
};
ActivePropEdit g_activeEdit;

void offerPropDrag(uint32_t entityId, const sProp& prop) {
	if (entityId == 0 || !prop.data) {
		return;
	}
	if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		return;
	}
	RigScenePropPayload payload;
	payload.entity = entityId;
	payload.propType = static_cast<int>(prop.type);
	std::snprintf(payload.name, sizeof(payload.name), "%s", prop.name.c_str());
	ImGui::SetDragDropPayload(kRigScenePropPayload, &payload, sizeof(payload));
	ImGui::Text("Ref → %s", prop.name.c_str());
	ImGui::EndDragDropSource();
}

} // namespace

PropValue readPropValue(const sProp& prop) {
	switch (prop.type) {
	case EPT_BOOL:
		return *static_cast<bool*>(prop.data);
	case EPT_INT:
	case EPT_ENUM:
	case EPT_UINT: // Edited through int, same as DragInt below.
		return *static_cast<int*>(prop.data);
	case EPT_FLOAT:
		return *static_cast<float*>(prop.data);
	case EPT_DOUBLE:
		return *static_cast<double*>(prop.data);
	case EPT_STRING:
		return *static_cast<std::string*>(prop.data);
	case EPT_VEC2:
		return *static_cast<glm::vec2*>(prop.data);
	case EPT_VEC3:
		return *static_cast<glm::vec3*>(prop.data);
	case EPT_VEC4:
	case EPT_COLOR:
		return *static_cast<glm::vec4*>(prop.data);
	default:
		return 0;
	}
}

void writePropValue(const sProp& prop, const PropValue& value) {
	if (!prop.data) {
		return;
	}
	switch (prop.type) {
	case EPT_BOOL:
		if (const auto* v = std::get_if<bool>(&value)) {
			*static_cast<bool*>(prop.data) = *v;
		}
		break;
	case EPT_INT:
	case EPT_ENUM:
	case EPT_UINT:
		if (const auto* v = std::get_if<int>(&value)) {
			*static_cast<int*>(prop.data) = *v;
		}
		break;
	case EPT_FLOAT:
		if (const auto* v = std::get_if<float>(&value)) {
			*static_cast<float*>(prop.data) = *v;
		}
		break;
	case EPT_DOUBLE:
		if (const auto* v = std::get_if<double>(&value)) {
			*static_cast<double*>(prop.data) = *v;
		}
		break;
	case EPT_STRING:
		if (const auto* v = std::get_if<std::string>(&value)) {
			*static_cast<std::string*>(prop.data) = *v;
		}
		break;
	case EPT_VEC2:
		if (const auto* v = std::get_if<glm::vec2>(&value)) {
			*static_cast<glm::vec2*>(prop.data) = *v;
		}
		break;
	case EPT_VEC3:
		if (const auto* v = std::get_if<glm::vec3>(&value)) {
			*static_cast<glm::vec3*>(prop.data) = *v;
		}
		break;
	case EPT_VEC4:
	case EPT_COLOR:
		if (const auto* v = std::get_if<glm::vec4>(&value)) {
			*static_cast<glm::vec4*>(prop.data) = *v;
		}
		break;
	default:
		break;
	}
}

bool RenderProps(const char* headerName, std::vector<sProp>& props, uint32_t entityId,
				 const PropCommitFn& onCommit) {
	if (props.empty()) {
		return false;
	}
	if (headerName) {
		if (!ImGui::CollapsingHeader(headerName, ImGuiTreeNodeFlags_DefaultOpen)) {
			return false;
		}
	}
	// Scope widget IDs per component so equal labels (e.g. "Inset (mm)" on both
	// CropmarkSettings and BorderSettings) do not collide in the same window.
	ImGui::PushID(headerName ? headerName : "props");

	bool anyChanged = false;
	for (auto& prop : props) {
		if (!prop.data) {
			continue;
		}
		// Read before the widget runs — on the activation frame this is still
		// the pre-edit value, even for same-frame edits like checkbox clicks.
		const PropValue pre = onCommit ? readPropValue(prop) : PropValue{};
		switch (prop.type) {
		case EPT_BOOL:
			anyChanged |=
				ImGui::Checkbox(prop.name.c_str(), static_cast<bool*>(prop.data));
			break;
		case EPT_INT:
			anyChanged |= ImGui::DragInt(prop.name.c_str(), static_cast<int*>(prop.data));
			break;
		case EPT_ENUM: {
			if (!prop.enumNames || prop.enumCount <= 0) {
				// A control that lies is worse than one that looks broken —
				// show the misconfiguration instead of a raw draggable int.
				ImGui::TextDisabled("%s (enum missing names)", prop.name.c_str());
				break;
			}
			auto* value = static_cast<int*>(prop.data);
			int idx = *value;
			if (idx < 0 || idx >= prop.enumCount) {
				idx = 0;
			}
			if (ImGui::Combo(prop.name.c_str(), &idx, prop.enumNames, prop.enumCount)) {
				*value = idx;
				anyChanged = true;
			}
			break;
		}
		case EPT_UINT:
			anyChanged |=
				ImGui::DragInt(prop.name.c_str(), static_cast<int*>(prop.data));
			break;
		case EPT_FLOAT:
			anyChanged |= ImGui::DragFloat(prop.name.c_str(),
										   static_cast<float*>(prop.data), 0.1f);
			break;
		case EPT_DOUBLE: {
			float f = static_cast<float>(*static_cast<double*>(prop.data));
			if (ImGui::DragFloat(prop.name.c_str(), &f, 0.1f)) {
				*static_cast<double*>(prop.data) = f;
				anyChanged = true;
			}
			break;
		}
		case EPT_STRING:
			anyChanged |= ImGuiInputTextStdString(
				prop.name.c_str(), *static_cast<std::string*>(prop.data));
			break;
		case EPT_VEC2: {
			auto* vec = static_cast<glm::vec2*>(prop.data);
			float arr[2] = {vec->x, vec->y};
			if (ImGui::DragFloat2(prop.name.c_str(), arr, 0.1f)) {
				vec->x = arr[0];
				vec->y = arr[1];
				anyChanged = true;
			}
			break;
		}
		case EPT_VEC3: {
			auto* vec = static_cast<glm::vec3*>(prop.data);
			float arr[3] = {vec->x, vec->y, vec->z};
			if (ImGui::DragFloat3(prop.name.c_str(), arr, 0.1f)) {
				vec->x = arr[0];
				vec->y = arr[1];
				vec->z = arr[2];
				anyChanged = true;
			}
			break;
		}
		case EPT_VEC4: {
			auto* vec = static_cast<glm::vec4*>(prop.data);
			float arr[4] = {vec->x, vec->y, vec->z, vec->w};
			if (ImGui::DragFloat4(prop.name.c_str(), arr, 0.1f)) {
				vec->x = arr[0];
				vec->y = arr[1];
				vec->z = arr[2];
				vec->w = arr[3];
				anyChanged = true;
			}
			break;
		}
		case EPT_COLOR: {
			auto* color = static_cast<glm::vec4*>(prop.data);
			ImVec4 imColor(color->r, color->g, color->b, color->a);
			if (ImGui::ColorEdit4(prop.name.c_str(), (float*)&imColor)) {
				color->r = imColor.x;
				color->g = imColor.y;
				color->b = imColor.z;
				color->a = imColor.w;
				anyChanged = true;
			}
			break;
		}
		default:
			ImGui::TextDisabled("%s (unsupported type)", prop.name.c_str());
			break;
		}
		// Query before offerPropDrag so "last item" is still the edit widget.
		if (onCommit) {
			const ImGuiID itemId = ImGui::GetItemID();
			if (ImGui::IsItemActivated()) {
				g_activeEdit = {true, itemId, prop.data, pre};
			}
			if (ImGui::IsItemDeactivatedAfterEdit() && g_activeEdit.valid &&
				g_activeEdit.itemId == itemId && g_activeEdit.field == prop.data) {
				onCommit({prop.id, prop.name, g_activeEdit.before, readPropValue(prop)});
				g_activeEdit.valid = false;
			}
		}
		offerPropDrag(entityId, prop);
	}
	ImGui::PopID();
	return anyChanged;
}

} // namespace rigkit
