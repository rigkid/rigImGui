#pragma once

namespace rigkit {

/**
 * @brief Evaluate a numeric field expression against the current value.
 * @details Leading `+` `*` `/` apply to the current value (`*2`, `+10`, `/2`).
 * `-10` sets a negative; `+-10` subtracts. `x` is the current value (`x*2`).
 * Full forms like `297*2` and `(100+50)/2` work. `+ - * / ( )` with usual
 * precedence. False on junk or divide-by-zero.
 * @param buf Expression text (already trimmed of leading blanks by the caller).
 * @param current Value in the field before the edit.
 * @param out Result when the function returns true.
 * @return true when @p out is a finite result of a complete expression.
 */
bool evalNumericExpr(const char* buf, double current, double& out);

/**
 * @brief Author number drag with click-to-type expressions (`*2`, `297*2`, `x+10`).
 * @details Prefer these over `ImGui::DragFloat` for authored values. Ctrl+click,
 * double-click, or click without drag opens a text field. Tab-focus then Enter,
 * Space, or typing a character also opens edit (typed digits replace the value).
 * Enter / deactivate runs `evalNumericExpr`. Drag uses `ImGuiSliderFlags_NoInput`
 * so stock TempInput cannot bypass the evaluator. Does not hook Dear ImGui.
 * Chrome / preview sliders may still use raw ImGui widgets.
 * @return true when the value changes (drag or committed expression).
 */
bool DragFloatExpr(const char* label, float* v, float speed = 1.f, float vMin = 0.f,
				   float vMax = 0.f, const char* format = "%.3f");

/** @brief Integer author drag; same activation and expression rules as DragFloatExpr. */
bool DragIntExpr(const char* label, int* v, float speed = 1.f, int vMin = 0, int vMax = 0,
				 const char* format = "%d");

/** @brief Double author drag; same activation and expression rules as DragFloatExpr. */
bool DragDoubleExpr(const char* label, double* v, float speed = 0.1f, double vMin = 0.0,
					double vMax = 0.0, const char* format = "%g");

/**
 * @brief Multi-component float drag (`DragFloat2` / `3` / `4` style).
 * @details Each component is a `DragFloatExpr`. Label is drawn to the right.
 */
bool DragFloatNExpr(const char* label, float* v, int components, float speed = 1.f,
					float vMin = 0.f, float vMax = 0.f, const char* format = "%.3f");

inline bool DragFloat2Expr(const char* label, float v[2], float speed = 1.f, float vMin = 0.f,
						   float vMax = 0.f, const char* format = "%.3f") {
	return DragFloatNExpr(label, v, 2, speed, vMin, vMax, format);
}

inline bool DragFloat3Expr(const char* label, float v[3], float speed = 1.f, float vMin = 0.f,
						   float vMax = 0.f, const char* format = "%.3f") {
	return DragFloatNExpr(label, v, 3, speed, vMin, vMax, format);
}

inline bool DragFloat4Expr(const char* label, float v[4], float speed = 1.f, float vMin = 0.f,
						   float vMax = 0.f, const char* format = "%.3f") {
	return DragFloatNExpr(label, v, 4, speed, vMin, vMax, format);
}

} // namespace rigkit
