#include "EntitiesWindow.h"

#include "core/RigKitEngine.h"
#include "ecs/MEcs.h"
#include "CSelection.h"
#include "EntityPick.h"

#include <imgui.h>
#include <cstdio>
#include <string>

namespace rigkit {

EntitiesWindow::EntitiesWindow(const std::string& title) : ImGuiPanel(title) {
	setCategory("Host");
}

void EntitiesWindow::renderContents() {
	auto* engine = getEngine();
	if (!engine || !engine->getECSManager()) {
		ImGui::TextDisabled("No ECS");
		return;
	}
	auto* ecs = engine->getECSManager();
	auto& reg = ecs->registry();

	uint32_t selectedId = static_cast<uint32_t>(-1);
	for (auto e : ecs->view<ecs::CSelection>()) {
		if (ecs->getComponent<ecs::CSelection>(e).isSelected && reg.valid(e)) {
			selectedId = entt::to_integral(e);
			break;
		}
	}

	bool any = false;
	for (auto entity : ecs->getAllEntities()) {
		if (!reg.valid(entity)) {
			continue;
		}
		any = true;
		const std::string name = ecs->entityName(entity);
		char label[256];
		std::snprintf(label, sizeof(label), "%s###ent_%u",
					  name.empty() ? "(unnamed)" : name.c_str(),
					  static_cast<unsigned>(entt::to_integral(entity)));
		const bool isSelected = (entt::to_integral(entity) == selectedId);
		if (ImGui::Selectable(label, isSelected)) {
			selectEntityOnly(*ecs, entity);
		}
	}
	if (!any) {
		ImGui::TextDisabled("No entities");
	}
}

} // namespace rigkit
