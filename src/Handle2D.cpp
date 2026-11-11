#include "Handle2D.h"

#include "core/util/UndoStack.h"
#include "CSelection.h"
#include "CTransform.h"
#include "PrimitiveBounds.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

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
	const glm::mat4 m = xf.localMatrix();
	auto at = [&](float lx, float ly) {
		const glm::vec4 p = m * glm::vec4(lx, ly, 0.f, 1.f);
		return ImVec2(originX + p.x * scale, originY + p.y * scale);
	};
	const ImVec2 corners[4] = {at(local.min.x, local.min.y), at(local.max.x, local.min.y),
							   at(local.max.x, local.max.y), at(local.min.x, local.max.y)};

	ImDrawList* dl = ImGui::GetBackgroundDrawList(ImGui::GetMainViewport());
	if (!dl) {
		return false;
	}
	dl->AddPolyline(corners, 4, IM_COL32(80, 180, 255, 220), ImDrawFlags_Closed, 1.5f);

	const float hs = 6.f;
	for (const auto& c : corners) {
		dl->AddRectFilled(ImVec2(c.x - hs, c.y - hs), ImVec2(c.x + hs, c.y + hs),
						  IM_COL32(80, 180, 255, 255));
	}
	const float x =
		std::min(std::min(corners[0].x, corners[1].x), std::min(corners[2].x, corners[3].x));
	const float y =
		std::min(std::min(corners[0].y, corners[1].y), std::min(corners[2].y, corners[3].y));
	const float w =
		std::max(std::max(corners[0].x, corners[1].x), std::max(corners[2].x, corners[3].x)) - x;
	const float h =
		std::max(std::max(corners[0].y, corners[1].y), std::max(corners[2].y, corners[3].y)) - y;

	if (op == IMui::GizmoOp::Select || ImGui::GetIO().KeyAlt || ImGui::GetIO().WantCaptureMouse) {
		return true;
	}

	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const bool inside =
		mouse.x >= x - hs && mouse.x <= x + w + hs && mouse.y >= y - hs && mouse.y <= y + h + hs;
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
