#include "UiControls.h"

#include "ecs/PropertyReflection.h"
#include "CName.h"
#include "CUiPanel.h"
#include "EntityProperty.h"
#include "PropEditors.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace rigkit {
namespace {

entt::entity findPanelByRole(MEcs& ecs, const char* role) {
	if (!role || !role[0]) {
		return entt::null;
	}
	for (auto e : ecs.view<ecs::CUiPanel>()) {
		if (ecs.getComponent<ecs::CUiPanel>(e).role == role) {
			return e;
		}
	}
	return entt::null;
}

std::string controlLabel(MEcs& ecs, entt::entity e, const ecs::CUiControl& c) {
	const std::string named = ecs.entityName(e);
	if (!named.empty()) {
		return named;
	}
	if (ecs.hasComponent<ecs::CName>(e)) {
		const auto& n = ecs.getComponent<ecs::CName>(e);
		if (!n.name.empty()) {
			return n.name;
		}
	}
	if (!c.propertyKey.empty()) {
		const auto dot = c.propertyKey.rfind('.');
		return dot == std::string::npos ? c.propertyKey : c.propertyKey.substr(dot + 1);
	}
	return "control";
}

bool drawOneControl(MEcs& ecs, entt::entity e, ecs::CUiControl& c, const UiControlDrawHost& host) {
	const entt::entity target = static_cast<entt::entity>(c.target);
	if (target == entt::null || c.propertyKey.empty()) {
		ImGui::TextDisabled("(unbound)");
		return false;
	}
	const std::string label = controlLabel(ecs, e, c);
	const bool disabled = !c.enabled || c.readOnly;
	if (disabled) {
		ImGui::BeginDisabled();
	}
	bool changed = false;
	if (c.type == "bool") {
		const auto cur = ecs::readEntityProperty(ecs, target, c.propertyKey);
		bool v = cur && *cur >= 0.5f;
		if (ImGui::Checkbox(label.c_str(), &v)) {
			ecs::writeEntityProperty(ecs, target, c.propertyKey, v ? 1.f : 0.f);
			changed = true;
		}
		offerScenePropDrag(entt::to_integral(target), c.propertyKey.c_str(), EPT_BOOL);
	} else if (c.type == "int" || c.type == "long") {
		const auto cur = ecs::readEntityProperty(ecs, target, c.propertyKey);
		int v = cur ? static_cast<int>(std::lround(*cur)) : 0;
		const int mn = static_cast<int>(c.min);
		const int mx = static_cast<int>(c.max);
		const float step = c.step > 0.f ? c.step : 1.f;
		if (ImGui::DragInt(label.c_str(), &v, step, mn, mx)) {
			ecs::writeEntityProperty(ecs, target, c.propertyKey, static_cast<float>(v));
			changed = true;
		}
		offerScenePropDrag(entt::to_integral(target), c.propertyKey.c_str(), EPT_INT);
	} else if (c.type == "color") {
		const std::string rKey = c.propertyKey + " R";
		const std::string gKey = c.propertyKey + " G";
		const std::string bKey = c.propertyKey + " B";
		const auto r = ecs::readEntityProperty(ecs, target, rKey);
		const auto g = ecs::readEntityProperty(ecs, target, gKey);
		const auto b = ecs::readEntityProperty(ecs, target, bKey);
		float rgb[3] = {r.value_or(1.f), g.value_or(1.f), b.value_or(1.f)};
		if (ImGui::ColorEdit3(label.c_str(), rgb)) {
			ecs::writeEntityProperty(ecs, target, rKey, rgb[0]);
			ecs::writeEntityProperty(ecs, target, gKey, rgb[1]);
			ecs::writeEntityProperty(ecs, target, bKey, rgb[2]);
			changed = true;
		}
		offerScenePropDrag(entt::to_integral(target), c.propertyKey.c_str(), EPT_COLOR);
	} else {
		const auto cur = ecs::readEntityProperty(ecs, target, c.propertyKey);
		float v = cur.value_or(0.f);
		const float step = c.step > 0.f ? c.step : 0.01f;
		if (c.widget == "slider") {
			if (ImGui::SliderFloat(label.c_str(), &v, c.min, c.max)) {
				ecs::writeEntityProperty(ecs, target, c.propertyKey, v);
				changed = true;
			}
		} else if (ImGui::DragFloat(label.c_str(), &v, step, c.min, c.max)) {
			ecs::writeEntityProperty(ecs, target, c.propertyKey, v);
			changed = true;
		}
		offerScenePropDrag(entt::to_integral(target), c.propertyKey.c_str(), EPT_FLOAT);
	}
	if (disabled) {
		ImGui::EndDisabled();
	}
	if (host.unpublish) {
		ImGui::SameLine();
		if (ImGui::SmallButton("x")) {
			host.unpublish(e);
		}
	}
	if (changed && host.onChanged) {
		host.onChanged();
	}
	return changed;
}

} // namespace

int drawUiPanelControls(MEcs& ecs, const char* role, const UiControlDrawHost& host) {
	const entt::entity panel = findPanelByRole(ecs, role);
	if (panel == entt::null) {
		return 0;
	}
	const uint32_t panelId = entt::to_integral(panel);
	struct Row {
		entt::entity e = entt::null;
		int order = 0;
	};
	std::vector<Row> rows;
	for (auto e : ecs.view<ecs::CUiControl>()) {
		auto& c = ecs.getComponent<ecs::CUiControl>(e);
		if (c.panel != panelId) {
			continue;
		}
		rows.push_back({e, c.order});
	}
	std::sort(rows.begin(), rows.end(),
			  [](const Row& a, const Row& b) { return a.order < b.order; });
	int drawn = 0;
	for (const auto& row : rows) {
		ImGui::PushID(static_cast<int>(entt::to_integral(row.e)));
		drawOneControl(ecs, row.e, ecs.getComponent<ecs::CUiControl>(row.e), host);
		ImGui::PopID();
		++drawn;
	}
	return drawn;
}

} // namespace rigkit
