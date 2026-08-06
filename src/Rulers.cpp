#include "Rulers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rigkit {
namespace {

float niceStep(float raw) {
	if (raw <= 0.f) {
		return 1.f;
	}
	const float exp = std::floor(std::log10(raw));
	const float base = std::pow(10.f, exp);
	const float n = raw / base;
	if (n <= 1.f) {
		return base;
	}
	if (n <= 2.f) {
		return 2.f * base;
	}
	if (n <= 5.f) {
		return 5.f * base;
	}
	return 10.f * base;
}

void formatTick(char* buf, size_t bufSize, float u, float stepUnits, const char* unitLabel,
				bool withSuffix) {
	const char* fmt = (stepUnits < 1.f) ? "%.1f" : "%.0f";
	if (withSuffix && unitLabel && unitLabel[0] != '\0') {
		char num[24];
		std::snprintf(num, sizeof(num), fmt, static_cast<double>(u));
		std::snprintf(buf, bufSize, "%s%s", num, unitLabel);
	} else {
		std::snprintf(buf, bufSize, fmt, static_cast<double>(u));
	}
}

} // namespace

float rulerPixPerDisplayUnit(RulerUnit unit, float contentZoomAbs, float dpiScale) {
	const float zoom = (contentZoomAbs > 1e-6f) ? contentZoomAbs : 1.f;
	const float dpi = (dpiScale > 0.1f) ? dpiScale : 1.f;
	const float ppi = 96.f * dpi;
	switch (unit) {
	case RulerUnit::Mm:
		return zoom * (ppi / 25.4f);
	case RulerUnit::Cm:
		return zoom * (ppi / 2.54f);
	case RulerUnit::In:
		return zoom * ppi;
	case RulerUnit::Px:
	default:
		return zoom;
	}
}

const char* rulerUnitLabel(RulerUnit unit) {
	switch (unit) {
	case RulerUnit::Mm:
		return "mm";
	case RulerUnit::Cm:
		return "cm";
	case RulerUnit::In:
		return "in";
	case RulerUnit::Px:
	default:
		return "px";
	}
}

float rulerStripThickness(float uiScale) {
	return 18.f * ((uiScale > 0.1f) ? uiScale : 1.f);
}

bool rulerHitStrip(ImVec2 origin, ImVec2 size, ImVec2 mouse, float uiScale) {
	if (size.x < 8.f || size.y < 8.f) {
		return false;
	}
	const float thick = rulerStripThickness(uiScale);
	const bool inX = mouse.x >= origin.x && mouse.x <= origin.x + size.x;
	const bool inY = mouse.y >= origin.y && mouse.y <= origin.y + size.y;
	if (!inX || !inY) {
		return false;
	}
	const bool onTop = mouse.y <= origin.y + thick;
	const bool onLeft = mouse.x <= origin.x + thick;
	return onTop || onLeft;
}

void drawRulersInRegion(ImDrawList* dl, ImVec2 origin, ImVec2 size, ImVec2 mouse,
						float pixPerUnit, const char* unitLabel, float uiScale,
						ImVec2 contentOrigin) {
	if (!dl || size.x < 8.f || size.y < 8.f || pixPerUnit <= 0.f) {
		return;
	}

	const float thick = rulerStripThickness(uiScale);
	const ImU32 bg = IM_COL32(40, 40, 44, 220);
	const ImU32 tick = IM_COL32(180, 180, 180, 220);
	const ImU32 text = IM_COL32(200, 200, 200, 255);

	dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + thick), bg);
	dl->AddRectFilled(origin, ImVec2(origin.x + thick, origin.y + size.y), bg);

	const float stepPx = niceStep(50.f * ((uiScale > 0.1f) ? uiScale : 1.f));
	const float stepUnits = stepPx / pixPerUnit;

	const float xMin = origin.x + thick;
	const float xMax = origin.x + size.x;
	const float yMin = origin.y + thick;
	const float yMax = origin.y + size.y;

	float uStartX = (xMin - contentOrigin.x) / pixPerUnit;
	uStartX = std::floor(uStartX / stepUnits) * stepUnits;
	float uStartY = (yMin - contentOrigin.y) / pixPerUnit;
	uStartY = std::floor(uStartY / stepUnits) * stepUnits;

	char label[40];
	const int maxTicks = 512;
	int drawn = 0;
	for (float u = uStartX; drawn < maxTicks; u += stepUnits, ++drawn) {
		const float x = contentOrigin.x + u * pixPerUnit;
		if (x > xMax) {
			break;
		}
		if (x < xMin) {
			continue;
		}
		dl->AddLine(ImVec2(x, origin.y + thick - 6.f), ImVec2(x, origin.y + thick), tick);
		formatTick(label, sizeof(label), u, stepUnits, unitLabel, true);
		dl->AddText(ImVec2(x + 2.f, origin.y + 2.f), text, label);
	}

	drawn = 0;
	for (float u = uStartY; drawn < maxTicks; u += stepUnits, ++drawn) {
		const float y = contentOrigin.y + u * pixPerUnit;
		if (y > yMax) {
			break;
		}
		if (y < yMin) {
			continue;
		}
		dl->AddLine(ImVec2(origin.x + thick - 6.f, y), ImVec2(origin.x + thick, y), tick);
		formatTick(label, sizeof(label), u, stepUnits, unitLabel, false);
		dl->AddText(ImVec2(origin.x + 2.f, y + 2.f), text, label);
	}

	if (mouse.x >= origin.x && mouse.x <= origin.x + size.x && mouse.y >= origin.y &&
		mouse.y <= origin.y + size.y) {
		dl->AddLine(ImVec2(mouse.x, origin.y), ImVec2(mouse.x, origin.y + thick),
					IM_COL32(255, 200, 80, 200));
		dl->AddLine(ImVec2(origin.x, mouse.y), ImVec2(origin.x + thick, mouse.y),
					IM_COL32(255, 200, 80, 200));
	}
}

bool rulerUnitPopup(const char* popupId, int& rulerUnit) {
	bool changed = false;
	if (ImGui::BeginPopup(popupId)) {
		ImGui::TextUnformatted("Ruler units");
		ImGui::Separator();
		for (int i = 0; i < kRulerUnitCount; ++i) {
			const bool selected = (rulerUnit == i);
			if (ImGui::Selectable(kRulerUnitNames[i], selected)) {
				if (rulerUnit != i) {
					rulerUnit = i;
					changed = true;
				}
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
	return changed;
}

} // namespace rigkit
