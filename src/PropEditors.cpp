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

bool RenderProps(const char* headerName, std::vector<sProp>& props, uint32_t entityId) {
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
		switch (prop.type) {
		case EPT_BOOL:
			anyChanged |=
				ImGui::Checkbox(prop.name.c_str(), static_cast<bool*>(prop.data));
			break;
		case EPT_INT: {
			auto* value = static_cast<int*>(prop.data);
			if (prop.enumNames && prop.enumCount > 0) {
				int idx = *value;
				if (idx < 0 || idx >= prop.enumCount) {
					idx = 0;
				}
				if (ImGui::Combo(prop.name.c_str(), &idx, prop.enumNames, prop.enumCount)) {
					*value = idx;
					anyChanged = true;
				}
			} else {
				anyChanged |= ImGui::DragInt(prop.name.c_str(), value);
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
		offerPropDrag(entityId, prop);
	}
	ImGui::PopID();
	return anyChanged;
}

} // namespace rigkit
