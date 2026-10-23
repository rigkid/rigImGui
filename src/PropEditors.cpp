#include "PropEditors.h"

#include "ImGuiExpr.h"
#include "SceneDragPayload.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <cstring>
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
	const bool changed = ImGui::DragScalar(label, ImGuiDataType_Double, v, 0.1f, nullptr, nullptr,
										   "%g", ImGuiSliderFlags_NoInput);
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
	if (entityId != kPropDragNoEntity) {
		ImGui::SetNextItemAllowOverlap();
	}
}

void EndPropDragSource() {
	if (!g_propDragStack.empty()) {
		g_propDragStack.pop_back();
	}
}

uint32_t currentPropDragEntity() {
	return g_propDragStack.empty() ? kPropDragNoEntity : g_propDragStack.back();
}

void offerScenePropDrag(const char* propName, int propType) {
	offerScenePropDrag(currentPropDragEntity(), propName, propType);
}

namespace {
struct PendingPatch {
	ScenePropPatchRequest req;
	int frame = 0;
};
std::vector<PendingPatch> g_patchRequests;
std::vector<ScenePropPatchSource> g_patchSources;

struct LfoShelf {
	const void* ecsTag = nullptr;
	std::vector<ScenePropLfoChoice> choices;
};
std::vector<LfoShelf> g_lfoShelves;

const std::vector<ScenePropLfoChoice>& lfoChoicesFor(const void* ecsTag) {
	static const std::vector<ScenePropLfoChoice> kEmpty;
	for (const LfoShelf& shelf : g_lfoShelves) {
		if (shelf.ecsTag == ecsTag) {
			return shelf.choices;
		}
	}
	return kEmpty;
}

/// Used when the node catalog has not registered outputs yet. Keep in step
/// with `mod.audio` in NodeCatalog.cpp.
struct SoundPin {
	const char* pin;
	const char* label;
};
constexpr SoundPin kSoundPins[] = {
	{"level", "Level"},	  {"bass", "Bass"},		  {"mid", "Mid"},
	{"treble", "Treble"}, {"peak", "Peak"},		  {"centroid", "Centroid"},
	{"spread", "Spread"}, {"rolloff", "Rolloff"}, {"flatness", "Flatness"},
	{"flux", "Flux"},	  {"onset", "Onset"},	  {"hue", "Hue"},
	{"warmth", "Warmth"},
};

void drawSoundPatchMenu(uint32_t entityId, const char* propName, int propType, const void* ecsTag,
						bool hasRange, float rangeMin, float rangeMax) {
	if (g_patchSources.empty()) {
		if (!ImGui::BeginMenu("Sound")) {
			return;
		}
		for (const SoundPin& pin : kSoundPins) {
			if (ImGui::MenuItem(pin.label)) {
				requestScenePropPatch(entityId, propName, propType, false, ecsTag, "mod.audio",
									  pin.pin, hasRange, rangeMin, rangeMax);
			}
		}
		ImGui::EndMenu();
		return;
	}
	std::vector<std::string> groups;
	for (const ScenePropPatchSource& src : g_patchSources) {
		bool seen = false;
		for (const std::string& group : groups) {
			if (group == src.group) {
				seen = true;
				break;
			}
		}
		if (!seen && !src.group.empty()) {
			groups.push_back(src.group);
		}
	}
	for (const std::string& group : groups) {
		if (!ImGui::BeginMenu(group.c_str())) {
			continue;
		}
		for (const ScenePropPatchSource& src : g_patchSources) {
			if (src.group != group || src.label.empty()) {
				continue;
			}
			if (ImGui::MenuItem(src.label.c_str())) {
				requestScenePropPatch(entityId, propName, propType, false, ecsTag,
									  src.typeId.c_str(), src.pin.c_str(), hasRange, rangeMin,
									  rangeMax);
			}
		}
		ImGui::EndMenu();
	}
}
} // namespace

void setScenePropPatchSources(std::vector<ScenePropPatchSource> sources) {
	g_patchSources = std::move(sources);
}

void setScenePropLfoChoices(const void* ecsTag, std::vector<ScenePropLfoChoice> choices) {
	for (LfoShelf& shelf : g_lfoShelves) {
		if (shelf.ecsTag == ecsTag) {
			shelf.choices = std::move(choices);
			return;
		}
	}
	g_lfoShelves.push_back(LfoShelf{ecsTag, std::move(choices)});
}

void requestScenePropPatch(uint32_t entityId, const char* propName, int propType, bool withLfo,
						   const void* ecsTag, const char* sourceType, const char* sourcePin,
						   bool hasMapRange, float mapOutMin, float mapOutMax, bool unpatch,
						   uint32_t reuseLfo) {
	if (entityId == kPropDragNoEntity || !propName || propName[0] == '\0') {
		return;
	}
	PendingPatch p;
	p.req.entity = entityId;
	p.req.name = propName;
	p.req.propType = propType;
	p.req.withLfo = withLfo;
	if (sourceType && sourceType[0] != '\0') {
		p.req.sourceType = sourceType;
		p.req.withLfo = false;
	}
	if (sourcePin && sourcePin[0] != '\0') {
		p.req.sourcePin = sourcePin;
	}
	p.req.hasMapRange = hasMapRange;
	p.req.mapOutMin = mapOutMin;
	p.req.mapOutMax = mapOutMax;
	p.req.unpatch = unpatch;
	p.req.ecsTag = ecsTag;
	p.req.reuseLfo = withLfo ? reuseLfo : 0;
	p.frame = ImGui::GetFrameCount();
	g_patchRequests.push_back(std::move(p));
}

std::vector<ScenePropPatchRequest> takeScenePropPatchRequests(const void* consumerEcs) {
	std::vector<ScenePropPatchRequest> out;
	std::vector<PendingPatch> keep;
	const int now = ImGui::GetFrameCount();
	for (auto& p : g_patchRequests) {
		// Drop stale entries queued while no editor was bound to this ECS.
		if (now - p.frame > 2) {
			continue;
		}
		// Tagged requests only go to the editor bound to that ECS (multi-deck
		// hosts run one editor per deck; entt ids collide across registries).
		const bool mine =
			p.req.ecsTag == nullptr || consumerEcs == nullptr || p.req.ecsTag == consumerEcs;
		if (mine) {
			out.push_back(std::move(p.req));
		} else {
			keep.push_back(std::move(p));
		}
	}
	g_patchRequests = std::move(keep);
	return out;
}

void offerScenePropDrag(uint32_t entityId, const char* propName, int propType, const void* ecsTag,
						float rangeMin, float rangeMax, bool hasRange) {
	if (entityId == kPropDragNoEntity) {
		entityId = currentPropDragEntity();
	}
	if (entityId == kPropDragNoEntity || !propName || propName[0] == '\0' ||
		!canPatchPropType(propType)) {
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
		// Alt+drag the value itself - DragFloat otherwise owns left-drag.
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
	if (hovered) {
		// Highlight the label so it reads as a draggable handle.
		dl->AddRectFilled(hit.Min, hit.Max, ImGui::GetColorU32(ImGuiCol_FrameBgHovered, 0.35f),
						  2.f);
	}
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
		payload.ecsTag = ecsTag;
		payload.hasRange = hasRange;
		payload.rangeMin = rangeMin;
		payload.rangeMax = rangeMax;
		std::snprintf(payload.name, sizeof(payload.name), "%s", propName);
		ImGui::SetDragDropPayload(kRigScenePropPayload, &payload, sizeof(payload));
		ImGui::Text("Patch to %s", propName);
		ImGui::TextDisabled("Drop on Node Editor or Patch Destination");
		ImGui::TextDisabled("Alt+drop adds an LFO, mapped onto this slider");
		ImGui::EndDragDropSource();
	} else if (hovered) {
		ImGui::SetTooltip("Drag to Node Editor or Patch Destination\nRight-click: new or existing "
						  "LFO, Sound, or Unpatch");
	}
	// Zero-drag path: right-click the handle to patch without drag-drop.
	if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
		ImGui::OpenPopup("##prop_patch_ctx");
	}
	if (ImGui::BeginPopup("##prop_patch_ctx")) {
		ImGui::TextDisabled("%s", propName);
		ImGui::Separator();
		if (ImGui::MenuItem("Patch to Node Editor")) {
			requestScenePropPatch(entityId, propName, propType, false, ecsTag);
		}
		const bool floatish = propType != EPT_VEC2 && propType != EPT_VEC4 && propType != EPT_COLOR;
		// LFO output is bipolar. The Map lands that span on this slider (0..1
		// when the widget has no limits of its own).
		const float outMin = hasRange ? rangeMin : 0.f;
		const float outMax = hasRange ? rangeMax : 1.f;
		if (floatish && ImGui::MenuItem("Patch with LFO")) {
			requestScenePropPatch(entityId, propName, propType, true, ecsTag, nullptr, nullptr,
								  true, outMin, outMax);
		}
		if (floatish) {
			const std::vector<ScenePropLfoChoice>& lfos = lfoChoicesFor(ecsTag);
			if (!lfos.empty() && ImGui::BeginMenu("Existing LFO")) {
				for (const ScenePropLfoChoice& lfo : lfos) {
					const std::string id = lfo.label + "##" + std::to_string(lfo.nodeId);
					if (ImGui::MenuItem(id.c_str())) {
						requestScenePropPatch(entityId, propName, propType, true, ecsTag, nullptr,
											  nullptr, true, outMin, outMax, false, lfo.nodeId);
					}
				}
				ImGui::EndMenu();
			}
		}
		// Submenu keeps the root menu to a few lines on a small screen.
		if (floatish) {
			drawSoundPatchMenu(entityId, propName, propType, ecsTag, hasRange, rangeMin, rangeMax);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Unpatch")) {
			requestScenePropPatch(entityId, propName, propType, false, ecsTag, nullptr, nullptr,
								  false, 0.f, 1.f, true);
		}
		ImGui::EndPopup();
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
	// Scope widget IDs per component so equal labels on two components
	// do not collide in the same window.
	ImGui::PushID(headerName ? headerName : "props");
	PropDragSource patchSrc(entityId);

	bool anyChanged = false;
	for (auto& prop : props) {
		if (!prop.data) {
			continue;
		}
		if (entityId != kPropDragNoEntity && canPatchPropType(static_cast<int>(prop.type))) {
			ImGui::SetNextItemAllowOverlap();
		}
		// Read before the widget runs - on the activation frame this is still
		// the pre-edit value, even for same-frame edits like checkbox clicks.
		const PropValue pre = onCommit ? readPropValue(prop) : PropValue{};
		switch (prop.type) {
		case EPT_BOOL:
			anyChanged |= ImGui::Checkbox(prop.name.c_str(), static_cast<bool*>(prop.data));
			break;
		case EPT_INT:
			anyChanged |= dragExprInt(prop.name.c_str(), static_cast<int*>(prop.data));
			break;
		case EPT_ENUM: {
			if (!prop.enumNames || prop.enumCount <= 0) {
				// A control that lies is worse than one that looks broken  -
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
			anyChanged |= dragExprFloat(prop.name.c_str(), static_cast<float*>(prop.data), 0.1f);
			break;
		case EPT_DOUBLE:
			anyChanged |= dragExprDouble(prop.name.c_str(), static_cast<double*>(prop.data));
			break;
		case EPT_STRING: {
			auto* value = static_cast<std::string*>(prop.data);
			if (prop.enumNames && prop.enumCount > 0) {
				int idx = 0;
				for (int i = 0; i < prop.enumCount; ++i) {
					if (prop.enumNames[i] && *value == prop.enumNames[i]) {
						idx = i;
						break;
					}
				}
				if (ImGui::Combo(prop.name.c_str(), &idx, prop.enumNames, prop.enumCount)) {
					*value = (idx >= 0 && idx < prop.enumCount && prop.enumNames[idx])
								 ? prop.enumNames[idx]
								 : std::string{};
					anyChanged = true;
				}
			} else {
				anyChanged |= ImGuiInputTextStdString(prop.name.c_str(), *value);
			}
			break;
		}
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
		case EPT_BUTTON: {
			auto* fn = static_cast<std::function<void()>*>(prop.data);
			if (ImGui::Button(prop.name.c_str()) && fn && *fn) {
				(*fn)();
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
