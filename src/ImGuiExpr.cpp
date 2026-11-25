#include "ImGuiExpr.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rigkit {
namespace {

void skipWs(const char*& s) {
	while (*s == ' ' || *s == '\t') {
		++s;
	}
}

bool parseExpr(const char*& s, double current, double& out);

bool parseNumber(const char*& s, double& out) {
	char* end = nullptr;
	out = std::strtod(s, &end);
	if (end == s) {
		return false;
	}
	s = end;
	return true;
}

bool parseCurrentX(const char*& s, double current, double& out) {
	if (*s != 'x' && *s != 'X') {
		return false;
	}
	const unsigned char next = static_cast<unsigned char>(s[1]);
	if (std::isalnum(next) || next == '_') {
		return false;
	}
	++s;
	out = current;
	return true;
}

bool parseFactor(const char*& s, double current, double& out) {
	skipWs(s);
	if (*s == '(') {
		++s;
		if (!parseExpr(s, current, out)) {
			return false;
		}
		skipWs(s);
		if (*s != ')') {
			return false;
		}
		++s;
		return true;
	}
	if (*s == '+') {
		++s;
		return parseFactor(s, current, out);
	}
	if (*s == '-') {
		++s;
		double v = 0.0;
		if (!parseFactor(s, current, v)) {
			return false;
		}
		out = -v;
		return true;
	}
	if (parseCurrentX(s, current, out)) {
		return true;
	}
	return parseNumber(s, out);
}

bool parseTermContinue(const char*& s, double current, double& out) {
	for (;;) {
		skipWs(s);
		if (*s != '*' && *s != '/') {
			break;
		}
		const char op = *s++;
		double rhs = 0.0;
		if (!parseFactor(s, current, rhs)) {
			return false;
		}
		if (op == '*') {
			out *= rhs;
		} else {
			if (rhs == 0.0) {
				return false;
			}
			out /= rhs;
		}
	}
	return true;
}

bool parseTerm(const char*& s, double current, double& out) {
	if (!parseFactor(s, current, out)) {
		return false;
	}
	return parseTermContinue(s, current, out);
}

bool parseExprContinue(const char*& s, double current, double& out) {
	for (;;) {
		skipWs(s);
		if (*s != '+' && *s != '-') {
			break;
		}
		const char op = *s++;
		double rhs = 0.0;
		if (!parseTerm(s, current, rhs)) {
			return false;
		}
		if (op == '+') {
			out += rhs;
		} else {
			out -= rhs;
		}
	}
	return true;
}

bool parseExpr(const char*& s, double current, double& out) {
	if (!parseTerm(s, current, out)) {
		return false;
	}
	return parseExprContinue(s, current, out);
}

/// One click-to-type buffer for author number fields. Drag stays stock ImGui;
/// we take over the text path so `*2` / `x*2` never needs a third_party hook.
struct ExprEdit {
	ImGuiID id = 0;
	bool focus = false;
	bool selectAll = true;
	double current = 0.0;
	char buf[64]{};
};
ExprEdit g_exprEdit;

void openExprEdit(ImGuiID id, double current, const char* seed, bool selectAll) {
	g_exprEdit.id = id;
	g_exprEdit.focus = true;
	g_exprEdit.selectAll = selectAll;
	g_exprEdit.current = current;
	std::snprintf(g_exprEdit.buf, sizeof(g_exprEdit.buf), "%s", seed ? seed : "");
	ImGui::ClearActiveID();
}

bool tryOpenFromTypedChars(ImGuiID id, double current, ImGuiIO& io) {
	if (io.InputQueueCharacters.Size <= 0) {
		return false;
	}
	char typed[64]{};
	size_t n = 0;
	for (int i = 0; i < io.InputQueueCharacters.Size && n + 1 < sizeof(typed); ++i) {
		const ImWchar c = io.InputQueueCharacters[i];
		if (c >= 32 && c < 127) {
			typed[n++] = static_cast<char>(c);
		}
	}
	if (n == 0) {
		return false;
	}
	typed[n] = '\0';
	io.InputQueueCharacters.resize(0);
	openExprEdit(id, current, typed, false);
	return true;
}

void tryOpenExprFromLastItem(double current, const char* seed) {
	if (g_exprEdit.id != 0) {
		return;
	}
	const ImGuiID id = ImGui::GetItemID();
	if (id == 0) {
		return;
	}
	ImGuiIO& io = ImGui::GetIO();
	const bool hover = ImGui::IsItemHovered();
	if (hover && (ImGui::IsMouseDoubleClicked(0) || (ImGui::IsMouseClicked(0) && io.KeyCtrl))) {
		openExprEdit(id, current, seed, true);
		return;
	}
	if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit() && io.MouseReleased[0] &&
		hover) {
		openExprEdit(id, current, seed, true);
		return;
	}
	if (!ImGui::IsItemFocused()) {
		return;
	}
	ImGuiContext& g = *GImGui;
	const bool navPreferInput =
		g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput) != 0;
	if (navPreferInput || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
		ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) || ImGui::IsKeyPressed(ImGuiKey_Space)) {
		openExprEdit(id, current, seed, true);
		return;
	}
	tryOpenFromTypedChars(id, current, io);
}

bool tickExprEdit(ImGuiID id, const char* label, double& out) {
	if (g_exprEdit.focus) {
		ImGui::SetKeyboardFocusHere();
		g_exprEdit.focus = false;
	}
	ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
	if (g_exprEdit.selectAll) {
		flags |= ImGuiInputTextFlags_AutoSelectAll;
	}
	ImGui::InputText(label, g_exprEdit.buf, sizeof(g_exprEdit.buf), flags);
	if (!ImGui::IsItemDeactivated()) {
		return false;
	}
	const bool ok = ImGui::IsItemDeactivatedAfterEdit() &&
					evalNumericExpr(g_exprEdit.buf, g_exprEdit.current, out);
	if (g_exprEdit.id == id) {
		g_exprEdit.id = 0;
	}
	return ok;
}

void clampFloat(float* v, float vMin, float vMax) {
	if (vMin < vMax) {
		if (*v < vMin) {
			*v = vMin;
		} else if (*v > vMax) {
			*v = vMax;
		}
	}
}

void clampInt(int* v, int vMin, int vMax) {
	if (vMin < vMax) {
		if (*v < vMin) {
			*v = vMin;
		} else if (*v > vMax) {
			*v = vMax;
		}
	}
}

void clampDouble(double* v, double vMin, double vMax) {
	if (vMin < vMax) {
		if (*v < vMin) {
			*v = vMin;
		} else if (*v > vMax) {
			*v = vMax;
		}
	}
}

} // namespace

bool evalNumericExpr(const char* buf, double current, double& out) {
	if (!buf) {
		return false;
	}
	const char* s = buf;
	skipWs(s);
	if (*s == '*' || *s == '/' || *s == '+') {
		out = current;
		if (*s == '*' || *s == '/') {
			if (!parseTermContinue(s, current, out) || !parseExprContinue(s, current, out)) {
				return false;
			}
		} else if (!parseExprContinue(s, current, out)) {
			return false;
		}
	} else if (!parseExpr(s, current, out)) {
		return false;
	}
	skipWs(s);
	return *s == '\0' && std::isfinite(out);
}

bool DragFloatExpr(const char* label, float* v, float speed, float vMin, float vMax,
				   const char* format) {
	if (!v || !label) {
		return false;
	}
	if (!format) {
		format = "%.3f";
	}
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (!window) {
		return false;
	}
	const ImGuiID id = window->GetID(label);
	if (g_exprEdit.id == id) {
		double out = 0.0;
		if (!tickExprEdit(id, label, out)) {
			return false;
		}
		*v = static_cast<float>(out);
		clampFloat(v, vMin, vMax);
		return true;
	}
	const bool changed =
		ImGui::DragFloat(label, v, speed, vMin, vMax, format, ImGuiSliderFlags_NoInput);
	// Plain number seed so unit suffixes in @p format ("%.0f%%", "%.1f mm") do not
	// break expression parse when the field opens for typing.
	char seed[64];
	std::snprintf(seed, sizeof(seed), "%g", static_cast<double>(*v));
	tryOpenExprFromLastItem(static_cast<double>(*v), seed);
	return changed;
}

bool DragIntExpr(const char* label, int* v, float speed, int vMin, int vMax, const char* format) {
	if (!v || !label) {
		return false;
	}
	if (!format) {
		format = "%d";
	}
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (!window) {
		return false;
	}
	const ImGuiID id = window->GetID(label);
	if (g_exprEdit.id == id) {
		double out = 0.0;
		if (!tickExprEdit(id, label, out)) {
			return false;
		}
		*v = static_cast<int>(std::llround(out));
		clampInt(v, vMin, vMax);
		return true;
	}
	const bool changed =
		ImGui::DragInt(label, v, speed, vMin, vMax, format, ImGuiSliderFlags_NoInput);
	char seed[64];
	std::snprintf(seed, sizeof(seed), "%d", *v);
	tryOpenExprFromLastItem(static_cast<double>(*v), seed);
	return changed;
}

bool DragDoubleExpr(const char* label, double* v, float speed, double vMin, double vMax,
					const char* format) {
	if (!v || !label) {
		return false;
	}
	if (!format) {
		format = "%g";
	}
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (!window) {
		return false;
	}
	const ImGuiID id = window->GetID(label);
	if (g_exprEdit.id == id) {
		double out = 0.0;
		if (!tickExprEdit(id, label, out)) {
			return false;
		}
		*v = out;
		clampDouble(v, vMin, vMax);
		return true;
	}
	const bool changed = ImGui::DragScalar(label, ImGuiDataType_Double, v, speed, &vMin, &vMax,
										   format, ImGuiSliderFlags_NoInput);
	char seed[64];
	std::snprintf(seed, sizeof(seed), "%g", *v);
	tryOpenExprFromLastItem(*v, seed);
	return changed;
}

bool DragFloatNExpr(const char* label, float* v, int components, float speed, float vMin,
					float vMax, const char* format) {
	if (!v || !label || components <= 0) {
		return false;
	}
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (!window) {
		return false;
	}
	ImGui::BeginGroup();
	ImGui::PushID(label);
	ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());
	bool changed = false;
	for (int i = 0; i < components; ++i) {
		ImGui::PushID(i);
		if (i > 0) {
			ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
		}
		changed |= DragFloatExpr("##v", &v[i], speed, vMin, vMax, format);
		ImGui::PopID();
		ImGui::PopItemWidth();
	}
	ImGui::PopID();
	const char* labelEnd = ImGui::FindRenderedTextEnd(label);
	if (label != labelEnd) {
		ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::TextEx(label, labelEnd);
	}
	ImGui::EndGroup();
	return changed;
}

} // namespace rigkit
