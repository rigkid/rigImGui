#include "PropEditors.h"

#include "ImGuiExpr.h"
#include "SceneDragPayload.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <vector>

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
std::vector<uint32_t> g_propDragStack;

/// Click-to-type buffer for one Properties number. Drag stays stock ImGui;
/// we take over the text path so `*2` / `x*2` never needs a third_party hook.
struct ExprEdit {
	ImGuiID id = 0;
	bool focus = false;
	double current = 0.0;
	char buf[64]{};
};
ExprEdit g_exprEdit;

void openExprEdit(ImGuiID id, double current, const char* seed) {
	g_exprEdit.id = id;
	g_exprEdit.focus = true;
	g_exprEdit.current = current;
	std::snprintf(g_exprEdit.buf, sizeof(g_exprEdit.buf), "%s", seed ? seed : "");
	ImGui::ClearActiveID();
}

void tryOpenExprFromLastItem(double current, const char* seed) {
	if (g_exprEdit.id != 0) {
		return;
	}
	const ImGuiID id = ImGui::GetItemID();
	if (id == 0) {
		return;
	}
	ImGuiIO& io = ImGui::GetIO();
	const bool hover = ImGui::IsItemHovered();
	if (hover && (ImGui::IsMouseDoubleClicked(0) || (ImGui::IsMouseClicked(0) && io.KeyCtrl))) {
		openExprEdit(id, current, seed);
		return;
	}
	if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit() && io.MouseReleased[0] &&
		hover) {
		openExprEdit(id, current, seed);
	}
}

bool tickExprEdit(ImGuiID id, const char* label, double& out) {
	if (g_exprEdit.focus) {
		ImGui::SetKeyboardFocusHere();
		g_exprEdit.focus = false;
	}
	ImGui::InputText(label, g_exprEdit.buf, sizeof(g_exprEdit.buf),
					 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
	if (!ImGui::IsItemDeactivated()) {
		return false;
	}
	const bool ok = ImGui::IsItemDeactivatedAfterEdit() &&
					evalNumericExpr(g_exprEdit.buf, g_exprEdit.current, out);
	if (g_exprEdit.id == id) {
		g_exprEdit.id = 0;
	}
	return ok;
}

bool dragExprFloat(const char* label, float* v, float speed) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (!window) {
		return false;
	}
	const ImGuiID id = window->GetID(label);
	if (g_exprEdit.id == id) {
		double out = 0.0;
		if (!tickExprEdit(id, label, out)) {
			return false;
		}
		*v = static_cast<float>(out);
		return true;
	}
	const bool changed =
		ImGui::DragFloat(label, v, speed, 0.f, 0.f, "%.3f", ImGuiSliderFlags_NoInput);
	char seed[64];
	std::snprintf(seed, sizeof(seed), "%.3f", *v);
	tryOpenExprFromLastItem(static_cast<double>(*v), seed);
	return changed;
}

bool dragExprInt(const char* label, int* v) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (!window) {
		return false;
	}
	const ImGuiID id = window->GetID(label);
	if (g_exprEdit.id == id) {
		double out = 0.0;
		if (!tickExprEdit(id, label, out)) {
			return false;
		}
		*v = static_cast<int>(std::llround(out));
		return true;
	}
	const bool changed = ImGui::DragInt(label, v, 1.f, 0, 0, "%d", ImGuiSliderFlags_NoInput);
	char seed[64];
	std::snprintf(seed, sizeof(seed), "%d", *v);
	tryOpenExprFromLastItem(static_cast<double>(*v), seed);
	return changed;
}

bool dragExprDouble(const char* label, double* v) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (!window) {
		return false;
	}
	const ImGuiID id = window->GetID(label);
	if (g_exprEdit.id == id) {
		double out = 0.0;
		if (!tickExprEdit(id, label, out)) {
			return false;
		}
		*v = out;
		return true;
	}
	const bool changed =
		ImGui::DragScalar(label, ImGuiDataType_Double, v, 0.1f, nullptr, nullptr, "%g",
						  ImGuiSliderFlags_NoInput);
	char seed[64];
	std::snprintf(seed, sizeof(seed), "%g", *v);
	tryOpenExprFromLastItem(*v, seed);
	return changed;
}

bool dragExprFloatN(const char* label, float* v, int n, float speed) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (!window || n <= 0) {
		return false;
	}
	ImGui::BeginGroup();
	ImGui::PushID(label);
	ImGui::PushMultiItemsWidths(n, ImGui::CalcItemWidth());
	bool changed = false;
	for (int i = 0; i < n; ++i) {
		ImGui::PushID(i);
		if (i > 0) {
			ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
		}
		changed |= dragExprFloat("##v", &v[i], speed);
		ImGui::PopID();
		ImGui::PopItemWidth();
	}
	ImGui::PopID();
	const char* labelEnd = ImGui::FindRenderedTextEnd(label);
	if (label != labelEnd) {
		ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextEx(label, labelEnd);
	}
	ImGui::EndGroup();
	return changed;
}

bool canPatchPropType(int propType) {
	switch (propType) {
	case EPT_BOOL:
	case EPT_INT:
	case EPT_ENUM:
	case EPT_UINT:
	case EPT_FLOAT:
	case EPT_DOUBLE:
	case EPT_VEC2:
	case EPT_VEC4:
	case EPT_COLOR:
		return true;
	default:
		return false;
	}
}

void offerPropDrag(uint32_t entityId, const sProp& prop) {
	if (!prop.data) {
		return;
	}
	offerScenePropDrag(entityId, prop.name.c_str(), static_cast<int>(prop.type));
}

} // namespace

void BeginPropDragSource(uint32_t entityId) {
	g_propDragStack.push_back(entityId);
	if (entityId != 0) {
		ImGui::SetNextItemAllowOverlap();
	}
}

void EndPropDragSource() {
	if (!g_propDragStack.empty()) {
		g_propDragStack.pop_back();
	}
}

uint32_t currentPropDragEntity() {
	return g_propDragStack.empty() ? 0 : g_propDragStack.back();
}

void offerScenePropDrag(const char* propName, int propType) {
	offerScenePropDrag(currentPropDragEntity(), propName, propType);
}

void offerScenePropDrag(uint32_t entityId, const char* propName, int propType) {
	if (entityId == 0) {
		entityId = currentPropDragEntity();
	}
	if (entityId == 0 || !propName || propName[0] == '\0' || !canPatchPropType(propType)) {
		return;
	}
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;
	if (!window || window->SkipItems || g.LastItemData.ID == 0) {
		return;
	}

	const ImRect total = g.LastItemData.Rect;
	const ImRect frame = g.LastItemData.NavRect;
	const bool hasLabel = total.Max.x > frame.Max.x + 1.f;
	ImRect hit;
	if (g.IO.KeyAlt) {
		// Alt+drag the value itself — DragFloat otherwise owns left-drag.
		hit = total;
	} else if (hasLabel) {
		// Field name (right of the frame). Drag that, not the number.
		hit = ImRect(ImVec2(frame.Max.x, frame.Min.y), ImVec2(total.Max.x, frame.Max.y));
	} else {
		hit = ImRect(frame.Min, ImVec2(frame.Min.x + ImGui::GetFrameHeight() * 0.45f, frame.Max.y));
	}
	if (hit.GetWidth() < 4.f || hit.GetHeight() < 4.f) {
		return;
	}

	// Let the overlay steal hover on the name without killing value-edit on the frame.
	ImGui::SetItemAllowOverlap();

	const ImVec2 restorePos = window->DC.CursorPos;
	const ImVec2 restoreMax = window->DC.CursorMaxPos;
	const ImVec2 restorePrev = window->DC.CursorPosPrevLine;
	const ImVec2 restorePrevSize = window->DC.PrevLineSize;
	const ImVec2 restoreCurrSize = window->DC.CurrLineSize;
	const float restoreBase = window->DC.CurrLineTextBaseOffset;
	const float restorePrevBase = window->DC.PrevLineTextBaseOffset;
	const bool restoreSame = window->DC.IsSameLine;
	const ImGuiLastItemData restoreItem = g.LastItemData;

	ImGui::SetCursorScreenPos(hit.Min);
	ImGui::PushID(static_cast<int>(entityId));
	ImGui::PushID(propName);
	ImGui::InvisibleButton("##prop_pin", hit.GetSize());
	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 pin((hit.Min.x + ImGui::GetStyle().ItemInnerSpacing.x + 4.f),
					 (hit.Min.y + hit.Max.y) * 0.5f);
	const float r = ImGui::GetFrameHeight() * 0.16f;
	auto* dl = ImGui::GetWindowDrawList();
	dl->AddCircleFilled(pin, r,
						ImGui::GetColorU32(hovered ? ImGuiCol_CheckMark : ImGuiCol_TextDisabled));
	dl->AddCircle(pin, r, ImGui::GetColorU32(ImGuiCol_Border), 0, 1.f);
	if (hovered) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		RigScenePropPayload payload;
		payload.entity = entityId;
		payload.propType = propType;
		std::snprintf(payload.name, sizeof(payload.name), "%s", propName);
		ImGui::SetDragDropPayload(kRigScenePropPayload, &payload, sizeof(payload));
		ImGui::Text("Patch → %s", propName);
		ImGui::TextDisabled("Drop on Node Editor");
		ImGui::TextDisabled("Alt+drop adds an LFO");
		ImGui::EndDragDropSource();
	} else if (hovered) {
		ImGui::SetTooltip("Drag to Node Editor to patch");
	}
	ImGui::PopID();
	ImGui::PopID();

	window->DC.CursorPos = restorePos;
	window->DC.CursorMaxPos = restoreMax;
	window->DC.CursorPosPrevLine = restorePrev;
	window->DC.PrevLineSize = restorePrevSize;
	window->DC.CurrLineSize = restoreCurrSize;
	window->DC.CurrLineTextBaseOffset = restoreBase;
	window->DC.PrevLineTextBaseOffset = restorePrevBase;
	window->DC.IsSameLine = restoreSame;
	g.LastItemData = restoreItem;
	ImGui::SetNextItemAllowOverlap();
}

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
	PropDragSource patchSrc(entityId);

	bool anyChanged = false;
	for (auto& prop : props) {
		if (!prop.data) {
			continue;
		}
		if (entityId != 0 && canPatchPropType(static_cast<int>(prop.type))) {
			ImGui::SetNextItemAllowOverlap();
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
			anyChanged |= dragExprInt(prop.name.c_str(), static_cast<int*>(prop.data));
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
			anyChanged |= dragExprInt(prop.name.c_str(), static_cast<int*>(prop.data));
			break;
		case EPT_FLOAT:
			anyChanged |=
				dragExprFloat(prop.name.c_str(), static_cast<float*>(prop.data), 0.1f);
			break;
		case EPT_DOUBLE:
			anyChanged |= dragExprDouble(prop.name.c_str(), static_cast<double*>(prop.data));
			break;
		case EPT_STRING:
			anyChanged |= ImGuiInputTextStdString(
				prop.name.c_str(), *static_cast<std::string*>(prop.data));
			break;
		case EPT_VEC2: {
			auto* vec = static_cast<glm::vec2*>(prop.data);
			float arr[2] = {vec->x, vec->y};
			if (dragExprFloatN(prop.name.c_str(), arr, 2, 0.1f)) {
				vec->x = arr[0];
				vec->y = arr[1];
				anyChanged = true;
			}
			break;
		}
		case EPT_VEC3: {
			auto* vec = static_cast<glm::vec3*>(prop.data);
			float arr[3] = {vec->x, vec->y, vec->z};
			if (dragExprFloatN(prop.name.c_str(), arr, 3, 0.1f)) {
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
			if (dragExprFloatN(prop.name.c_str(), arr, 4, 0.1f)) {
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
