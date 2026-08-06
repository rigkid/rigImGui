#include "EntityPick.h"

#include "CSelectable.h"
#include "CSelection.h"
#include "CShape.h"
#include "CTransform.h"
#include <algorithm>
#include <cmath>

namespace rigkit {
namespace {

bool isSelectable(MEcs& ecs, entt::entity e) {
	if (!ecs.hasComponent<ecs::CSelectable>(e)) {
		return true; // legacy: no flag → selectable
	}
	return ecs.getComponent<ecs::CSelectable>(e).enabled;
}

} // namespace

entt::entity pickEntityAt(MEcs& ecs, float contentX, float contentY, float maxDist) {
	entt::entity best = entt::null;
	float bestDist = maxDist;

	for (auto e : ecs.view<ecs::CTransform, ecs::CShape>()) {
		if (!isSelectable(ecs, e)) {
			continue;
		}
		const auto& xf = ecs.getComponent<ecs::CTransform>(e);
		const auto& shape = ecs.getComponent<ecs::CShape>(e);
		const float x1 = xf.position.x + shape.x1;
		const float y1 = xf.position.y + shape.y1;
		const float x2 = xf.position.x + shape.x2;
		const float y2 = xf.position.y + shape.y2;
		const float cx = (std::max)(x1, (std::min)(contentX, x2));
		const float cy = (std::max)(y1, (std::min)(contentY, y2));
		const float dx = contentX - cx;
		const float dy = contentY - cy;
		const float d = std::sqrt(dx * dx + dy * dy);
		if (d <= bestDist) {
			bestDist = d;
			best = e;
		}
	}
	return best;
}

void selectEntityOnly(MEcs& ecs, entt::entity e) {
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

} // namespace rigkit
