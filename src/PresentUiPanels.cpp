#include "PresentUiPanels.h"

#include "CUiPanel.h"
#include "EntityProperty.h"
#include "PropEditors.h"
#include "ecs/MEcs.h"
#include "ecs/systems/SEvent.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace rigkit {
namespace {

struct UiItem {
	enum class Kind { Group, Control, Action } kind = Kind::Control;
	int order = 0;
	entt::entity entity = entt::null;
};

std::string itemLabel(MEcs& ecs, entt::entity entity, const std::string& fallback) {
	std::string name = ecs.entityName(entity);
	if (name.empty()) {
		name = fallback;
	}
	if (name.empty()) {
		name = "item";
	}
	return name;
}

void drawReadonly(const sProp& prop, const char* label) {
	if (prop.type == EPT_FLOAT || prop.type == EPT_DOUBLE) {
		ImGui::Text("%s: %.3f", label, *static_cast<float*>(prop.data));
		return;
	}
	if (prop.type == EPT_INT || prop.type == EPT_UINT || prop.type == EPT_ENUM) {
		ImGui::Text("%s: %d", label, *static_cast<int*>(prop.data));
		return;
	}
	if (prop.type == EPT_BOOL) {
		ImGui::Text("%s: %s", label, *static_cast<bool*>(prop.data) ? "true" : "false");
		return;
	}
	if (prop.type == EPT_STRING) {
		ImGui::Text("%s: %s", label, static_cast<std::string*>(prop.data)->c_str());
		return;
	}
	ImGui::TextDisabled("%s", label);
}

void drawStringOptions(sProp& prop, const ecs::CUiControl& ctrl, const char* label) {
	auto* value = static_cast<std::string*>(prop.data);
	int current = 0;
	for (int i = 0; i < static_cast<int>(ctrl.options.size()); ++i) {
		if (ctrl.options[static_cast<size_t>(i)] == *value) {
			current = i;
			break;
		}
	}
	if (ImGui::BeginCombo(label, value->c_str())) {
		for (int i = 0; i < static_cast<int>(ctrl.options.size()); ++i) {
			const bool selected = i == current;
			if (ImGui::Selectable(ctrl.options[static_cast<size_t>(i)].c_str(), selected)) {
				*value = ctrl.options[static_cast<size_t>(i)];
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
}

void drawControl(MEcs& ecs, entt::entity entity, const ecs::CUiControl& ctrl) {
	const std::string label = itemLabel(ecs, entity, ctrl.propertyKey);
	const auto target = static_cast<entt::entity>(ctrl.target);
	if (ctrl.target == 0 || !ecs.registry().valid(target)) {
		ImGui::TextDisabled("%s (no target)", label.c_str());
		return;
	}
	auto found = ecs::findEntityProperty(ecs, target, ctrl.propertyKey);
	if (!found) {
		ImGui::TextDisabled("%s (missing)", label.c_str());
		return;
	}
	sProp prop = *found;
	prop.name = label;

	const bool locked = !ctrl.enabled || ctrl.readOnly;
	if (locked) {
		ImGui::BeginDisabled();
	}
	if (ctrl.readOnly) {
		drawReadonly(prop, label.c_str());
	} else if (!ctrl.options.empty() && prop.type == EPT_STRING) {
		drawStringOptions(prop, ctrl, label.c_str());
	} else if ((ctrl.widget == ecs::kUiWidgetSlider || ctrl.widget == ecs::kUiWidgetKnob) &&
			   (prop.type == EPT_FLOAT || prop.type == EPT_DOUBLE)) {
		float lo = ctrl.min;
		float hi = ctrl.max;
		if (hi < lo) {
			std::swap(lo, hi);
		}
		ImGui::SliderFloat(label.c_str(), static_cast<float*>(prop.data), lo, hi);
	} else if (ctrl.widget == ecs::kUiWidgetToggle && prop.type == EPT_BOOL) {
		ImGui::Checkbox(label.c_str(), static_cast<bool*>(prop.data));
	} else if (ctrl.widget == ecs::kUiWidgetField &&
			   (prop.type == EPT_FLOAT || prop.type == EPT_DOUBLE)) {
		ImGui::InputFloat(label.c_str(), static_cast<float*>(prop.data));
	} else if (ctrl.widget == ecs::kUiWidgetColor &&
			   (prop.type == EPT_COLOR || prop.type == EPT_VEC4)) {
		ImGui::ColorEdit4(label.c_str(), static_cast<float*>(prop.data));
	} else if (ctrl.widget == ecs::kUiWidgetXy && prop.type == EPT_VEC2) {
		ImGui::DragFloat2(label.c_str(), static_cast<float*>(prop.data),
						  ctrl.step > 0.f ? ctrl.step : 0.01f);
	} else {
		std::vector<sProp> one{prop};
		RenderProps(nullptr, one, ctrl.target);
	}
	if (locked) {
		ImGui::EndDisabled();
	}
}

void drawAction(MEcs& ecs, entt::entity entity, const ecs::CUiAction& action) {
	if (action.actionId.empty() || !ecs.getEventSystem().hasAction(action.actionId)) {
		return;
	}
	const std::string label = itemLabel(ecs, entity, action.actionId);
	if (!action.enabled) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button(label.c_str())) {
		ecs.getEventSystem().triggerAction(action.actionId);
	}
	if (!action.enabled) {
		ImGui::EndDisabled();
	}
}

void collectItems(MEcs& ecs, uint32_t panelId, uint32_t groupId, std::vector<UiItem>& out) {
	auto& reg = ecs.registry();
	for (auto e : reg.view<ecs::CUiGroup>()) {
		const auto& g = reg.get<ecs::CUiGroup>(e);
		if (g.panel == panelId && g.parent == groupId) {
			out.push_back({UiItem::Kind::Group, g.order, e});
		}
	}
	for (auto e : reg.view<ecs::CUiControl>()) {
		const auto& c = reg.get<ecs::CUiControl>(e);
		if (c.panel == panelId && c.group == groupId) {
			out.push_back({UiItem::Kind::Control, c.order, e});
		}
	}
	for (auto e : reg.view<ecs::CUiAction>()) {
		const auto& a = reg.get<ecs::CUiAction>(e);
		if (a.panel == panelId && a.group == groupId) {
			out.push_back({UiItem::Kind::Action, a.order, e});
		}
	}
	std::sort(out.begin(), out.end(), [](const UiItem& a, const UiItem& b) {
		if (a.order != b.order) {
			return a.order < b.order;
		}
		return entt::to_integral(a.entity) < entt::to_integral(b.entity);
	});
}

void drawItems(MEcs& ecs, uint32_t panelId, uint32_t groupId, bool horizontal);

void drawGroup(MEcs& ecs, entt::entity entity, const ecs::CUiGroup& group) {
	const std::string title = itemLabel(ecs, entity, "Group");
	const bool horizontal = group.orientation == ecs::kUiOrientHorizontal;
	if (horizontal) {
		drawItems(ecs, group.panel, entt::to_integral(entity), true);
		return;
	}
	const ImGuiTreeNodeFlags flags =
		group.collapsed ? 0 : ImGuiTreeNodeFlags_DefaultOpen;
	if (ImGui::CollapsingHeader(title.c_str(), flags)) {
		drawItems(ecs, group.panel, entt::to_integral(entity), false);
	}
}

void drawItems(MEcs& ecs, uint32_t panelId, uint32_t groupId, bool horizontal) {
	std::vector<UiItem> items;
	collectItems(ecs, panelId, groupId, items);
	auto& reg = ecs.registry();
	for (size_t i = 0; i < items.size(); ++i) {
		const auto& item = items[i];
		ImGui::PushID(static_cast<int>(entt::to_integral(item.entity)));
		if (item.kind == UiItem::Kind::Group && reg.all_of<ecs::CUiGroup>(item.entity)) {
			drawGroup(ecs, item.entity, reg.get<ecs::CUiGroup>(item.entity));
		} else if (item.kind == UiItem::Kind::Control &&
				   reg.all_of<ecs::CUiControl>(item.entity)) {
			drawControl(ecs, item.entity, reg.get<ecs::CUiControl>(item.entity));
		} else if (item.kind == UiItem::Kind::Action &&
				   reg.all_of<ecs::CUiAction>(item.entity)) {
			drawAction(ecs, item.entity, reg.get<ecs::CUiAction>(item.entity));
		}
		ImGui::PopID();
		if (horizontal && i + 1 < items.size()) {
			ImGui::SameLine();
		}
	}
}

} // namespace

void presentUiPanels(MEcs& ecs) {
	auto& reg = ecs.registry();
	std::vector<entt::entity> panels;
	for (auto e : reg.view<ecs::CUiPanel>()) {
		if (reg.get<ecs::CUiPanel>(e).visible) {
			panels.push_back(e);
		}
	}
	std::sort(panels.begin(), panels.end(), [&](entt::entity a, entt::entity b) {
		const int ao = reg.get<ecs::CUiPanel>(a).order;
		const int bo = reg.get<ecs::CUiPanel>(b).order;
		if (ao != bo) {
			return ao < bo;
		}
		return entt::to_integral(a) < entt::to_integral(b);
	});

	for (auto e : panels) {
		auto& panel = reg.get<ecs::CUiPanel>(e);
		const uint32_t panelId = entt::to_integral(e);
		std::string title = itemLabel(ecs, e, panel.role.empty() ? "Panel" : panel.role);
		char id[64];
		std::snprintf(id, sizeof(id), "##rig.ui.panel.%u", panelId);
		title += id;

		ImGuiWindowFlags flags = 0;
		if (panel.surface == ecs::kUiSurfaceHud) {
			flags |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize;
			if (const ImGuiViewport* vp = ImGui::GetMainViewport()) {
				ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12.f, vp->WorkPos.y + 12.f),
										ImGuiCond_FirstUseEver);
			}
		} else if (panel.preferredWidth > 0.f && panel.preferredHeight > 0.f) {
			ImGui::SetNextWindowSize(ImVec2(panel.preferredWidth, panel.preferredHeight),
									 ImGuiCond_FirstUseEver);
		}

		bool open = panel.visible;
		if (ImGui::Begin(title.c_str(), &open, flags)) {
			drawItems(ecs, panelId, 0, false);
		}
		ImGui::End();
		panel.visible = open;
	}
}

} // namespace rigkit
