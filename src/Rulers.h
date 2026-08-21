#pragma once

#include <imgui.h>

namespace rigkit {

/// Display unit for host / Viewport rulers (persisted as UiPrefs.rulerUnit).
enum class RulerUnit { Px = 0, Mm, Cm, In };

inline constexpr int kRulerUnitCount = 4;

inline RulerUnit clampRulerUnit(int unit) {
	if (unit < 0 || unit >= kRulerUnitCount) {
		return RulerUnit::Px;
	}
	return static_cast<RulerUnit>(unit);
}

inline constexpr const char* const kRulerUnitNames[] = {"Px", "Mm", "Cm", "In"};

/**
 * @brief Screen pixels per display unit for content that is measured in pixels.
 * @param contentZoomAbs View2D::zoomAbs (1 = identity screen mapping).
 * @param dpiScale Window content scale (HiDPI); used with 96 CSS PPI.
 */
float rulerPixPerDisplayUnit(RulerUnit unit, float contentZoomAbs, float dpiScale);

/**
 * @brief Screen pixels per display unit when 1 world unit is 1 cm (CAD / 3D).
 * @param pixelsPerWorldCm How many view pixels a 1 cm world offset covers at the focus.
 * @details Px falls back to identity screen pixels. `pixelsPerWorldCm <= 0` uses the
 * paper mapping (`rulerPixPerDisplayUnit` at zoom 1).
 */
float rulerPixPerWorldCm(RulerUnit unit, float pixelsPerWorldCm, float dpiScale);

const char* rulerUnitLabel(RulerUnit unit);

float rulerStripThickness(float uiScale = 1.f);

/// True if mouse is over the top or left ruler strip (corner counts as both).
bool rulerHitStrip(ImVec2 origin, ImVec2 size, ImVec2 mouse, float uiScale = 1.f);

/**
 * @brief Draw top/left ruler strips over a screen region.
 * @param pixPerUnit Screen pixels per display unit.
 * @param contentOrigin Screen position of content (0,0) so ticks track pan/zoom.
 */
void drawRulersInRegion(ImDrawList* dl, ImVec2 origin, ImVec2 size, ImVec2 mouse,
						float pixPerUnit, const char* unitLabel, float uiScale = 1.f,
						ImVec2 contentOrigin = ImVec2(0, 0));

/**
 * @brief Right-click unit picker popup (call every frame; OpenPopup when strip hit).
 * @return true if the unit value changed.
 */
bool rulerUnitPopup(const char* popupId, int& rulerUnit);

} // namespace rigkit
