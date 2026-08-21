#include "AnchorPad.h"

#include "UiDpi.h"
#include <algorithm>
#include <cstring>
#include <imgui.h>
#include <string>

namespace rigkit {
namespace {

const char* kFaceTips[9] = {"Top Left",	  "Top Center",	  "Top Right",
							"Middle Left",	  "Center",		  "Middle Right",
							"Bottom Left", "Bottom Center", "Bottom Right"};
const char* kHeightTips[3] = {"Min Z", "Center Z", "Max Z"};

ImU32 colU32(ImGuiCol idx, float mul = 1.f) {
	ImVec4 c = ImGui::GetStyleColorVec4(idx);
	c.x = std::min(1.f, c.x * mul);
	c.y = std::min(1.f, c.y * mul);
	c.z = std::min(1.f, c.z * mul);
	return ImGui::ColorConvertFloat4ToU32(c);
}

void drawFacePip(ImDrawList* dl, ImVec2 mn, ImVec2 mx, int face, ImU32 fill, float scale) {
	const float w = mx.x - mn.x;
	const float h = mx.y - mn.y;
	const int col = face % 3;
	const int row = face / 3;
	const float u = 0.22f + 0.28f * static_cast<float>(col);
	const float v = 0.22f + 0.28f * static_cast<float>(row);
	const float r = std::min(w, h) * 0.14f * scale;
	dl->AddCircleFilled(ImVec2(mn.x + w * u, mn.y + h * v), r, fill, 12);
}

void drawHeightPip(ImDrawList* dl, ImVec2 mn, ImVec2 mx, int height, ImU32 fill, ImU32 dim) {
	const float w = mx.x - mn.x;
	const float h = mx.y - mn.y;
	const float pad = h * 0.16f;
	const float slabH = (h - pad * 2.f) / 3.f - 1.f;
	for (int i = 0; i < 3; ++i) {
		const int cellH = 2 - i; // top slab = max
		const float y0 = mn.y + pad + static_cast<float>(i) * (slabH + 1.5f);
		const bool on = cellH == height;
		const ImU32 c = on ? fill : dim;
		dl->AddRectFilled(ImVec2(mn.x + w * 0.22f, y0), ImVec2(mx.x - w * 0.22f, y0 + slabH), c,
						  1.5f);
	}
}

bool cellButton(const char* id, ImVec2 size, bool selected, bool depthCol, int facePip,
				int heightPip, const char* tip) {
	const ImVec2 p = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton(id, size);
	const bool hovered = ImGui::IsItemHovered();
	const bool clicked = ImGui::IsItemClicked();
	if (hovered && tip) {
		ImGui::SetTooltip("%s", tip);
	}

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 q(p.x + size.x, p.y + size.y);
	const float rnd = uiPx(3.f);
	ImU32 bg = colU32(ImGuiCol_FrameBg);
	if (selected) {
		bg = ImGui::GetColorU32(ImGuiCol_SliderGrab);
	} else if (hovered) {
		bg = colU32(ImGuiCol_FrameBgHovered);
	} else if (depthCol) {
		ImVec4 c = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
		c.x *= 0.78f;
		c.y *= 0.88f;
		c.z = std::min(1.f, c.z * 1.12f);
		bg = ImGui::ColorConvertFloat4ToU32(c);
	}
	dl->AddRectFilled(p, q, bg, rnd);
	const ImU32 border = selected ? ImGui::GetColorU32(ImGuiCol_SliderGrabActive)
								  : ImGui::GetColorU32(ImGuiCol_Border);
	dl->AddRect(p, q, border, rnd, 0, selected ? 2.f : 1.f);

	const ImU32 pip = selected ? ImGui::GetColorU32(ImGuiCol_Text)
							  : ImGui::GetColorU32(ImGuiCol_Text, 0.45f);
	const ImU32 dim = ImGui::GetColorU32(ImGuiCol_Text, 0.16f);
	if (depthCol) {
		drawHeightPip(dl, p, q, heightPip, pip, dim);
	} else {
		drawFacePip(dl, p, q, facePip, pip, selected ? 1.35f : 1.f);
	}
	return clicked;
}

std::string stateCaption(int faceSel, int heightSel, bool showFace, bool showHeight) {
	std::string s;
	if (showFace) {
		s = (faceSel >= 0) ? kFaceTips[faceSel] : "Custom";
	}
	if (showHeight) {
		if (!s.empty()) {
			s += " · ";
		}
		s += (heightSel >= 0) ? kHeightTips[heightSel] : "Custom Z";
	}
	return s;
}

} // namespace

AnchorPadHit AnchorPad(const char* label, int* face, int* height) {
	if (!label || (!face && !height)) {
		return AnchorPadHit::None;
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const float cell = uiPx(18.f);
	const float gap = uiPx(2.f);
	const float zGap = uiPx(6.f);
	const ImVec2 cellSize(cell, cell);

	ImGui::PushID(label);
	ImGui::BeginGroup();
	ImGui::BeginGroup();
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));

	AnchorPadHit hit = AnchorPadHit::None;
	const int faceSel = (face && *face >= 0 && *face < 9) ? *face : -1;
	const int heightSel = (height && *height >= 0 && *height < 3) ? *height : -1;

	for (int row = 0; row < 3; ++row) {
		if (face) {
			for (int col = 0; col < 3; ++col) {
				if (col > 0) {
					ImGui::SameLine();
				}
				const int idx = row * 3 + col;
				ImGui::PushID(idx);
				if (cellButton("##f", cellSize, idx == faceSel, false, idx, 0, kFaceTips[idx])) {
					*face = idx;
					hit = AnchorPadHit::Face;
				}
				ImGui::PopID();
			}
		}
		if (height) {
			if (face) {
				ImGui::SameLine(0.f, zGap);
			}
			const int hIdx = 2 - row; // top = max
			ImGui::PushID(20 + hIdx);
			if (cellButton("##h", cellSize, hIdx == heightSel, true, 0, hIdx, kHeightTips[hIdx])) {
				*height = hIdx;
				hit = AnchorPadHit::Height;
			}
			ImGui::PopID();
		}
	}

	ImGui::PopStyleVar();
	ImGui::EndGroup();

	const char* hash = std::strstr(label, "##");
	const char* textEnd = hash ? hash : label + std::strlen(label);
	if (textEnd != label && *label != '\0') {
		ImGui::SameLine(0.f, style.ItemInnerSpacing.x);
		ImGui::BeginGroup();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label, textEnd);
		const int capFace = (face && *face >= 0 && *face < 9) ? *face : -1;
		const int capHeight = (height && *height >= 0 && *height < 3) ? *height : -1;
		const std::string cap =
			stateCaption(capFace, capHeight, face != nullptr, height != nullptr);
		ImGui::TextDisabled("%s", cap.c_str());
		ImGui::EndGroup();
	}

	ImGui::EndGroup();
	ImGui::PopID();
	return hit;
}

} // namespace rigkit
