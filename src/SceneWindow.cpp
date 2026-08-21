#include "SceneWindow.h"

#include "CRelationship.h"
#include "CSelection.h"
#include "MWindow.h"
#include "PropertiesWindow.h"
#include "SceneDragPayload.h"
#include "core/IMui.h"
#include "core/RigKitEngine.h"
#include "ecs/MEcs.h"
#include <cstdio>
#include <functional>
#include <imgui.h>
#include <unordered_set>
#include <vector>

namespace rigkit {

SceneWindow::SceneWindow(const std::string& title) : IWindow(title) {
	setCategory("Host");
}

void SceneWindow::selectOnly(entt::entity e) {
	auto* engine = getEngine();
	if (!engine) {
		return;
	}
	auto* ecs = engine->getECSManager();
	if (!ecs) {
		return;
	}
	auto& reg = ecs->registry();
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
	if (auto* ui = engine->getUiManager()) {
		if (auto* wm = ui->getWindowManager()) {
			if (auto props = wm->getWindow<PropertiesWindow>("Properties")) {
				props->setSelectedEntity(e == entt::null ? PropertiesWindow::kNoEntity
														 : entt::to_integral(e));
			}
		}
	}
}

void SceneWindow::renderContents() {
	auto* engine = getEngine();
	if (!engine || !engine->getECSManager()) {
		ImGui::TextDisabled("No ECS");
		return;
	}
	auto* ecs = engine->getECSManager();
	auto& reg = ecs->registry();

	std::unordered_set<entt::entity> hasParent;
	for (auto e : reg.view<ecs::CRelationship>()) {
		const auto parent = reg.get<ecs::CRelationship>(e).parent;
		if (parent != entt::null && reg.valid(parent)) {
			hasParent.insert(e);
		}
	}

	std::vector<entt::entity> roots;
	for (auto e : ecs->getAllEntities()) {
		if (!reg.valid(e) || hasParent.count(e)) {
			continue;
		}
		const std::string name = ecs->entityName(e);
		if (!name.empty() || reg.all_of<ecs::CSelection>(e) ||
			reg.all_of<ecs::CRelationship>(e)) {
			roots.push_back(e);
		}
	}

	if (roots.empty()) {
		ImGui::TextDisabled("No entities");
		return;
	}

	entt::entity dropParent = entt::null;
	entt::entity dragChild = entt::null;
	bool dropAsRoot = false;

	std::function<void(entt::entity)> drawNode;
	drawNode = [&](entt::entity e) {
		const std::string name = ecs->entityName(e);
		char label[256];
		std::snprintf(label, sizeof(label), "%s###ent_%u",
					  name.empty() ? "(unnamed)" : name.c_str(),
					  static_cast<unsigned>(entt::to_integral(e)));

		const bool selected = reg.all_of<ecs::CSelection>(e) &&
							  reg.get<ecs::CSelection>(e).isSelected;

		ImGuiTreeNodeFlags flags =
			ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
			(selected ? ImGuiTreeNodeFlags_Selected : 0);

		std::vector<entt::entity> children;
		for (auto child : reg.view<ecs::CRelationship>()) {
			if (reg.get<ecs::CRelationship>(child).parent == e) {
				children.push_back(child);
			}
		}
		if (children.empty()) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		const bool open = ImGui::TreeNodeEx(label, flags);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
			selectOnly(e);
		}

		if (ImGui::BeginDragDropSource()) {
			const uint32_t id = entt::to_integral(e);
			ImGui::SetDragDropPayload(kRigSceneEntityPayload, &id, sizeof(id));
			ImGui::TextUnformatted(label);
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload =
					ImGui::AcceptDragDropPayload(kRigSceneEntityPayload)) {
				const uint32_t id = *static_cast<const uint32_t*>(payload->Data);
				dragChild = static_cast<entt::entity>(id);
				dropParent = e;
			}
			ImGui::EndDragDropTarget();
		}

		if (open && !children.empty()) {
			for (auto c : children) {
				drawNode(c);
			}
			ImGui::TreePop();
		}
	};

	for (auto e : roots) {
		drawNode(e);
	}

	// Drop on empty space → reparent to root.
	ImGui::Dummy(ImVec2(0, 8));
	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kRigSceneEntityPayload)) {
			const uint32_t id = *static_cast<const uint32_t*>(payload->Data);
			dragChild = static_cast<entt::entity>(id);
			dropAsRoot = true;
		}
		ImGui::EndDragDropTarget();
	}
	ImGui::TextDisabled("Drag onto a node to parent; drop here for root.");

	if (dragChild != entt::null && reg.valid(dragChild)) {
		if (dropAsRoot) {
			if (reg.all_of<ecs::CRelationship>(dragChild)) {
				reg.get<ecs::CRelationship>(dragChild).parent = entt::null;
			}
		} else if (dropParent != entt::null && dropParent != dragChild &&
				   reg.valid(dropParent)) {
			// Avoid parenting to own descendant (cheap one-level guard via walk).
			bool cyclic = false;
			entt::entity walk = dropParent;
			while (walk != entt::null && reg.all_of<ecs::CRelationship>(walk)) {
				walk = reg.get<ecs::CRelationship>(walk).parent;
				if (walk == dragChild) {
					cyclic = true;
					break;
				}
			}
			if (!cyclic) {
				if (!reg.all_of<ecs::CRelationship>(dragChild)) {
					reg.emplace<ecs::CRelationship>(dragChild, ecs::CRelationship{});
				}
				reg.get<ecs::CRelationship>(dragChild).parent = dropParent;
			}
		}
	}
}

} // namespace rigkit
