#include "Handle2D.h"

#include "CSelection.h"
#include "CTransform.h"
#include "PrimitiveBounds.h"

#include <imgui.h>

namespace rigkit {
namespace {

entt::entity firstSelected(MEcs& ecs) {
	for (auto e : ecs.view<ecs::CSelection, ecs::CTransform>()) {
		const auto& sel = ecs.getComponent<ecs::CSelection>(e);
		if ((sel.isSelected || sel.isMultiSelected) && ecs::hasShape2D(ecs, e)) {
			return e;
		}
	}
	return entt::null;
}

} // namespace

bool drawSelectedHandle2D(MEcs& ecs, float originX, float originY, float scale) {
	if (scale <= 0.f) {
		return false;
	}
	const entt::entity e = firstSelected(ecs);
	if (e == entt::null) {
		return false;
	}

	auto& xf = ecs.getComponent<ecs::CTransform>(e);
	const ecs::Bounds2D local = ecs::shapeBounds2D(ecs, e);
	const float x = originX + (xf.position.x + local.min.x) * scale;
	const float y = originY + (xf.position.y + local.min.y) * scale;
	const float w = local.width() * scale;
	const float h = local.height() * scale;

	ImDrawList* dl = ImGui::GetForegroundDrawList();
	dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(80, 180, 255, 220), 0.f, 0, 1.5f);

	const float hs = 6.f;
	const ImVec2 corners[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
	for (const auto& c : corners) {
		dl->AddRectFilled(ImVec2(c.x - hs, c.y - hs), ImVec2(c.x + hs, c.y + hs),
						  IM_COL32(80, 180, 255, 255));
	}

	// Drag body to translate (when left button held over bbox).
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const bool inside = mouse.x >= x && mouse.x <= x + w && mouse.y >= y && mouse.y <= y + h;
	if (inside && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt) {
		xf.position.x += ImGui::GetIO().MouseDelta.x / scale;
		xf.position.y += ImGui::GetIO().MouseDelta.y / scale;
	}

	return true;
}

} // namespace rigkit
