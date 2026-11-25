#include "InputSpin.h"

#include "ImGuiExpr.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace rigkit {
namespace {

void addSaturating(int* v, int delta) {
	const long long next = static_cast<long long>(*v) + delta;
	if (next > INT_MAX) {
		*v = INT_MAX;
	} else if (next < INT_MIN) {
		*v = INT_MIN;
	} else {
		*v = static_cast<int>(next);
	}
}

// One half of the spin column. The mark is drawn, not a glyph, so it fits
// in half a frame (a full-size "+" is taller than that).
bool spinHalf(const char* id, ImVec2 size, bool plus, ImDrawFlags corners) {
	const ImVec2 p = ImGui::GetCursorScreenPos();
	ImGui::PushID(id);
	ImGui::PushButtonRepeat(true);
	const bool pressed = ImGui::InvisibleButton("##h", size);
	ImGui::PopButtonRepeat();
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	ImGui::PopID();

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 q(p.x + size.x, p.y + size.y);
	ImGuiCol bgCol = ImGuiCol_Button;
	if (held && hovered) {
		bgCol = ImGuiCol_ButtonActive;
	} else if (hovered) {
		bgCol = ImGuiCol_ButtonHovered;
	}
	const ImU32 bg = ImGui::GetColorU32(bgCol);
	const float rounding = ImGui::GetStyle().FrameRounding;
	dl->AddRectFilled(p, q, bg, rounding, corners);
	if (ImGui::GetStyle().FrameBorderSize > 0.f) {
		dl->AddRect(p, q, ImGui::GetColorU32(ImGuiCol_Border), rounding, corners,
					ImGui::GetStyle().FrameBorderSize);
	}

	const float arm = std::max(2.f, size.y * 0.28f);
	const float thick = std::max(1.f, size.y * 0.12f);
	const float cx = (p.x + q.x) * 0.5f;
	const float cy = (p.y + q.y) * 0.5f;
	const ImU32 fg = ImGui::GetColorU32(ImGuiCol_Text);
	dl->AddLine(ImVec2(cx - arm, cy), ImVec2(cx + arm, cy), fg, thick);
	if (plus) {
		dl->AddLine(ImVec2(cx, cy - arm), ImVec2(cx, cy + arm), fg, thick);
		dl->AddLine(ImVec2(p.x, q.y), ImVec2(q.x, q.y), ImGui::GetColorU32(ImGuiCol_Border), 1.f);
	}
	return pressed;
}

struct SpinExprEdit {
	ImGuiID id = 0;
	double current = 0.0;
	char buf[64]{};
};
SpinExprEdit g_spinExpr;

} // namespace

bool InputSpinInt(const char* label, int* v, int step, int stepFast) {
	if (!v || !label) {
		return false;
	}
	if (step == 0) {
		step = 1;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const float h = ImGui::GetFrameHeight();
	const float topH = std::floor(h * 0.5f);
	const float botH = h - topH;
	const float bw = h;
	const float gap = style.ItemInnerSpacing.x;

	ImGui::BeginGroup();
	ImGui::PushID(label);
	ImGui::SetNextItemWidth(std::max(1.f, ImGui::CalcItemWidth() - bw - gap));

	ImGuiWindow* window = ImGui::GetCurrentWindow();
	const ImGuiID fieldId = window ? window->GetID("##v") : 0;
	bool changed = false;
	const ImGuiInputTextFlags textFlags =
		ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
	if (g_spinExpr.id == fieldId) {
		ImGui::InputText("##v", g_spinExpr.buf, sizeof(g_spinExpr.buf), textFlags);
		if (ImGui::IsItemDeactivated()) {
			double out = 0.0;
			if (ImGui::IsItemDeactivatedAfterEdit() &&
				evalNumericExpr(g_spinExpr.buf, g_spinExpr.current, out)) {
				*v = static_cast<int>(std::llround(out));
				changed = true;
			}
			g_spinExpr.id = 0;
		}
	} else {
		char seed[64];
		std::snprintf(seed, sizeof(seed), "%d", *v);
		ImGui::InputText("##v", seed, sizeof(seed), textFlags);
		if (ImGui::IsItemActivated()) {
			g_spinExpr.id = fieldId;
			g_spinExpr.current = static_cast<double>(*v);
			std::snprintf(g_spinExpr.buf, sizeof(g_spinExpr.buf), "%s", seed);
		} else if (ImGui::IsItemDeactivatedAfterEdit()) {
			double out = 0.0;
			if (evalNumericExpr(seed, static_cast<double>(*v), out)) {
				*v = static_cast<int>(std::llround(out));
				changed = true;
			}
		}
	}

	ImGui::SameLine(0.f, gap);
	ImGui::BeginGroup();
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
	if (spinHalf("up", ImVec2(bw, topH), true, ImDrawFlags_RoundCornersTop)) {
		addSaturating(v, ImGui::GetIO().KeyCtrl && stepFast != 0 ? stepFast : step);
		changed = true;
	}
	if (spinHalf("down", ImVec2(bw, botH), false, ImDrawFlags_RoundCornersBottom)) {
		addSaturating(v, -(ImGui::GetIO().KeyCtrl && stepFast != 0 ? stepFast : step));
		changed = true;
	}
	ImGui::PopStyleVar();
	ImGui::EndGroup();

	const char* end = label;
	while (*end && !(end[0] == '#' && end[1] == '#')) {
		++end;
	}
	if (end != label) {
		ImGui::SameLine(0.f, gap);
		ImGui::TextUnformatted(label, end);
	}

	ImGui::PopID();
	ImGui::EndGroup();
	return changed;
}

} // namespace rigkit
