#include "LayersWindow.h"

#include "CLayer.h"
#include "CSelection.h"
#include "core/RigKitEngine.h"
#include "ecs/MEcs.h"
#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <vector>

namespace rigkit {
namespace {

void selectOnly(MEcs& ecs, entt::entity e) {
	auto& reg = ecs.registry();
	for (auto ent : reg.view<ecs::CSelection>()) {
		auto& sel = reg.get<ecs::CSelection>(ent);
		sel.isSelected = (ent == e);
		sel.isMultiSelected = false;
		sel.selectionIndex = (ent == e) ? 0 : -1;
	}
	if (e != entt::null && !reg.all_of<ecs::CSelection>(e)) {
		ecs::CSelection sel;
		sel.isSelected = true;
		sel.selectionIndex = 0;
		reg.emplace<ecs::CSelection>(e, sel);
	}
}

} // namespace

LayersWindow::LayersWindow(const std::string& title) : IWindow(title) {
	setCategory("Host");
}

void LayersWindow::renderContents() {
	auto* engine = getEngine();
	if (!engine || !engine->getECSManager()) {
		ImGui::TextDisabled("No ECS");
		return;
	}
	auto* ecs = engine->getECSManager();
	auto& reg = ecs->registry();

	struct Row {
		entt::entity e;
		int order;
	};
	std::vector<Row> rows;
	for (auto e : reg.view<ecs::CLayer>()) {
		rows.push_back({e, reg.get<ecs::CLayer>(e).order});
	}
	std::sort(rows.begin(), rows.end(),
			  [](const Row& a, const Row& b) { return a.order > b.order; });

	if (rows.empty()) {
		ImGui::TextDisabled("No CLayer entities");
		ImGui::TextWrapped("Add ecs::CLayer to entities to manage them here.");
		return;
	}

	int dragFrom = -1;
	int dragTo = -1;

	for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
		auto e = rows[static_cast<size_t>(i)].e;
		auto& layer = reg.get<ecs::CLayer>(e);
		const std::string name = ecs->entityName(e);
		const bool selected = reg.all_of<ecs::CSelection>(e) &&
							  reg.get<ecs::CSelection>(e).isSelected;

		ImGui::PushID(static_cast<int>(entt::to_integral(e)));
		ImGui::Checkbox("##vis", &layer.visible);
		ImGui::SameLine();
		ImGui::Checkbox("##lock", &layer.locked);
		ImGui::SameLine();

		char label[256];
		std::snprintf(label, sizeof(label), "%s###layer",
					  name.empty() ? "(layer)" : name.c_str());
		if (ImGui::Selectable(label, selected)) {
			selectOnly(*ecs, e);
		}

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("RIG_LAYER_ORDER", &i, sizeof(i));
			ImGui::TextUnformatted(label);
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload("RIG_LAYER_ORDER")) {
				dragFrom = *static_cast<const int*>(payload->Data);
				dragTo = i;
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::PopID();
	}

	if (dragFrom >= 0 && dragTo >= 0 && dragFrom != dragTo) {
		auto fromE = rows[static_cast<size_t>(dragFrom)].e;
		auto toE = rows[static_cast<size_t>(dragTo)].e;
		const int tmp = reg.get<ecs::CLayer>(fromE).order;
		reg.get<ecs::CLayer>(fromE).order = reg.get<ecs::CLayer>(toE).order;
		reg.get<ecs::CLayer>(toE).order = tmp;
	}
}

} // namespace rigkit
