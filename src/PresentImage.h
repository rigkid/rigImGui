#pragma once

/**
 * @file PresentImage.h
 * @brief ImGui blit of a GL color texture using RigKit TextureYOrigin.
 * @details Packs must not invent V-flip UVs. Call this (or
 * `presentUvForScreenTopLeft` + AddImage) so 2D TopLeft and 3D BottomLeft
 * attachments both present upright.
 */

#include "rendering/TextureYOrigin.h"

#include <imgui.h>

namespace rigkit {

/**
 * @brief Draw a GL texture into an ImGui draw list with upright screen Y.
 * @param origin Writer convention of @p texId (`Canvas::colorYOrigin()`,
 *        `MeshPresentTarget::kColorYOrigin`, or TopLeft for CPU uploads).
 * @param uMax / @p vMax Used subrect in [0,1] when the viewport is smaller than the FBO.
 */
inline void addImagePresent(ImDrawList* dl, unsigned texId, ImVec2 p0, ImVec2 p1,
							TextureYOrigin origin, float uMax = 1.f, float vMax = 1.f,
							ImU32 col = IM_COL32_WHITE) {
	if (!dl || !texId) {
		return;
	}
	const PresentUv uv = presentUvForScreenTopLeft(origin, uMax, vMax);
	dl->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(texId)), p0, p1,
				 ImVec2(uv.u0, uv.v0), ImVec2(uv.u1, uv.v1), col);
}

} // namespace rigkit
