#pragma once

#include <algorithm>
#include <cmath>

#include <imgui.h>

namespace rigkit {

/**
 * @brief Live ImGui content scale after Mui::applyDpiStyle.
 * @details Reads style.FontScaleDpi — no stored mirror. Valid inside an ImGui
 * frame; returns 1 when there is no context or the scale is unset/tiny.
 */
inline float uiDpi() {
	if (!ImGui::GetCurrentContext()) {
		return 1.f;
	}
	const float dpi = ImGui::GetStyle().FontScaleDpi;
	return (dpi < 0.5f) ? 1.f : dpi;
}

/** @brief Design (1x) size → ImGui/Display units. */
inline float uiPx(float design) {
	return design * uiDpi();
}

/** @brief Design width/height → ImGui/Display ImVec2. */
inline ImVec2 uiSize(float designW, float designH) {
	const float dpi = uiDpi();
	return ImVec2(designW * dpi, designH * dpi);
}

/**
 * @brief Clamp a display size into the main viewport work area.
 * @param size Display-unit size (already passed through uiPx / uiSize).
 * @param padDesign Margin from work edges in 1x design units.
 */
inline ImVec2 uiClampToWork(ImVec2 size, float padDesign = 40.f) {
	if (!ImGui::GetCurrentContext()) {
		return size;
	}
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	if (!vp) {
		return size;
	}
	const float pad = uiPx(padDesign);
	const float maxW = std::max(1.f, vp->WorkSize.x - pad);
	const float maxH = std::max(1.f, vp->WorkSize.y - pad);
	size.x = std::clamp(size.x, 1.f, maxW);
	size.y = std::clamp(size.y, 1.f, maxH);
	return size;
}

/**
 * @brief Size for an InvisibleButton hit box, never zero on either axis.
 * @details A docked panel measures zero while a layout settles — restoring a
 * layout saved on a wider screen is enough to do it — and InvisibleButton
 * asserts on a zero dimension. One pixel keeps the hit box alive until the
 * panel has room again.
 */
inline ImVec2 uiHitSize(ImVec2 size) {
	size.x = std::max(1.f, size.x);
	size.y = std::max(1.f, size.y);
	return size;
}

/** @brief Design size → int window size, clamped to the work area. */
inline void uiWindowSize(int designW, int designH, int& outW, int& outH,
						 float padDesign = 40.f) {
	const ImVec2 clamped =
		uiClampToWork(uiSize(static_cast<float>(designW), static_cast<float>(designH)), padDesign);
	outW = std::max(1, static_cast<int>(std::lround(clamped.x)));
	outH = std::max(1, static_cast<int>(std::lround(clamped.y)));
}

} // namespace rigkit
