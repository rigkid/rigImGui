#include "Handle2D.h"

#include "CSelection.h"
#include "CTransform.h"
#include "PrimitiveBounds.h"
#include "core/util/UndoStack.h"

#include <cmath>
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

struct DragUndo {
	bool active = false;
	entt::entity e{entt::null};
	ecs::CTransform before{};
	IMui::GizmoOp op = IMui::GizmoOp::Select;
};

DragUndo& dragUndo() {
	static DragUndo d;
	return d;
}

const char* opLabel(IMui::GizmoOp op) {
	switch (op) {
	case IMui::GizmoOp::Rotate:
		return "Rotate";
	case IMui::GizmoOp::Scale:
		return "Scale";
	default:
		return "Move";
	}
}

void commitDrag(MEcs& ecs, UndoStack* undo) {
	auto& du = dragUndo();
	if (!du.active) {
		return;
	}
	if (undo && ecs.hasComponent<ecs::CTransform>(du.e)) {
		const auto after = ecs.getComponent<ecs::CTransform>(du.e);
		const auto e = du.e;
		const auto before = du.before;
		undo->pushSnapshot(opLabel(du.op), before, after,
						   [ecs = &ecs, e](const ecs::CTransform& t) {
							   if (ecs->hasComponent<ecs::CTransform>(e)) {
								   ecs->getComponent<ecs::CTransform>(e) = t;
							   }
						   });
	}
	du = {};
}

} // namespace

bool drawSelectedHandle2D(MEcs& ecs, float originX, float originY, float scale, IMui::GizmoOp op,
						  UndoStack* undo) {
	if (scale <= 0.f) {
		return false;
	}
	if (dragUndo().active && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		commitDrag(ecs, undo);
	}

	const entt::entity e = firstSelected(ecs);
	if (e == entt::null) {
		return false;
	}

	auto& xf = ecs.getComponent<ecs::CTransform>(e);
	const ecs::Bounds2D local = ecs::shapeBounds2D(ecs, e);
	if (!local.valid) {
		return false;
	}
	const float x = originX + (xf.position.x + local.min.x) * scale;
	const float y = originY + (xf.position.y + local.min.y) * scale;
	const float w = local.width() * scale;
	const float h = local.height() * scale;

	ImDrawList* dl = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
	if (!dl) {
		return false;
	}
	dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(80, 180, 255, 220), 0.f, 0, 1.5f);

	const float hs = 6.f;
	const ImVec2 corners[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
	for (const auto& c : corners) {
		dl->AddRectFilled(ImVec2(c.x - hs, c.y - hs), ImVec2(c.x + hs, c.y + hs),
						  IM_COL32(80, 180, 255, 255));
	}

	if (op == IMui::GizmoOp::Select || ImGui::GetIO().KeyAlt || ImGui::GetIO().WantCaptureMouse) {
		return true;
	}

	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const bool inside = mouse.x >= x - hs && mouse.x <= x + w + hs && mouse.y >= y - hs &&
						mouse.y <= y + h + hs;
	if (!inside || !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		return true;
	}

	if (!dragUndo().active) {
		dragUndo().active = true;
		dragUndo().e = e;
		dragUndo().before = xf;
		dragUndo().op = op;
	}

	const float dx = ImGui::GetIO().MouseDelta.x / scale;
	const float dy = ImGui::GetIO().MouseDelta.y / scale;
	if (op == IMui::GizmoOp::Translate) {
		xf.position.x += dx;
		xf.position.y += dy;
	} else if (op == IMui::GizmoOp::Rotate) {
		const float cx = xf.position.x + local.center().x;
		const float cy = xf.position.y + local.center().y;
		const auto content = ImGui::GetIO().MousePos;
		const float px = (content.x - originX) / scale - cx;
		const float py = (content.y - originY) / scale - cy;
		const float pdx = px - dx;
		const float pdy = py - dy;
		const float a0 = std::atan2(pdy, pdx);
		const float a1 = std::atan2(py, px);
		xf.setEulerRadians({xf.euler.x, xf.euler.y, xf.euler.z + (a1 - a0)});
	} else if (op == IMui::GizmoOp::Scale) {
		const float factor = 1.f + (dx + dy) * 0.05f;
		if (factor > 0.05f) {
			xf.scale *= factor;
		}
	}

	return true;
}

} // namespace rigkit
