#include "DualColorSlots.h"

#include <IconsFontAwesome5.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace rigkit {
namespace {

ImU32 contrastingBorder(const glm::vec4& c) {
	const float lum = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
	return lum > 0.55f ? IM_COL32(40, 40, 45, 255) : IM_COL32(230, 230, 235, 255);
}

ImU32 toImU32(const glm::vec4& c) {
	return IM_COL32(static_cast<int>(std::lround(std::clamp(c.r, 0.f, 1.f) * 255.f)),
					static_cast<int>(std::lround(std::clamp(c.g, 0.f, 1.f) * 255.f)),
					static_cast<int>(std::lround(std::clamp(c.b, 0.f, 1.f) * 255.f)),
					static_cast<int>(std::lround(std::clamp(c.a, 0.f, 1.f) * 255.f)));
}

void drawChecker(ImDrawList* dl, ImVec2 min, ImVec2 max) {
	const ImU32 a = IM_COL32(180, 180, 180, 90);
	const ImU32 b = IM_COL32(90, 90, 90, 90);
	constexpr float s = 4.f;
	for (float y = min.y; y < max.y; y += s) {
		for (float x = min.x; x < max.x; x += s) {
			const int ix = static_cast<int>((x - min.x) / s);
			const int iy = static_cast<int>((y - min.y) / s);
			dl->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + s, max.x), std::min(y + s, max.y)),
							  ((ix + iy) & 1) ? a : b);
		}
	}
}

void drawChip(ImDrawList* dl, ImVec2 min, ImVec2 max, const glm::vec4& color, bool active,
			  const char* label) {
	if (color.a < 0.98f) {
		drawChecker(dl, min, max);
	}
	dl->AddRectFilled(min, max, toImU32(color), 2.f);
	const ImU32 border = active ? IM_COL32(70, 160, 255, 255) : contrastingBorder(color);
	dl->AddRect(min, max, border, 2.f, 0, active ? 2.5f : 1.25f);
	if (label && ImGui::IsMouseHoveringRect(min, max)) {
		ImGui::SetTooltip("%s", label);
	}
}

bool chipClicked(ImVec2 min, ImVec2 max) {
	return ImGui::IsMouseHoveringRect(min, max) && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

bool chipDoubleClicked(ImVec2 min, ImVec2 max) {
	return ImGui::IsMouseHoveringRect(min, max) &&
		   ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
}

} // namespace

DualColorEdit drawDualColorSlots(glm::vec4& front, glm::vec4& back, int& active, ImVec2 size,
								 const char* frontLabel, const char* backLabel,
								 const DualColorSlotsOpts& opts) {
	DualColorEdit edit;
	const char* frontName = frontLabel && frontLabel[0] ? frontLabel : "Front";
	const char* backName = backLabel && backLabel[0] ? backLabel : "Back";
	ImGui::PushID(frontName);

	const float w = std::max(8.f, size.x);
	const float h = std::max(8.f, size.y);
	ImGui::InvisibleButton("##dualHit", ImVec2(w, h));
	const ImVec2 origin = ImGui::GetItemRectMin();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	const float chip = std::min(w, h) * 0.62f;
	const float inset = std::max(1.f, chip * 0.08f);
	const ImVec2 backMin(origin.x + w - chip - inset, origin.y + h - chip - inset);
	const ImVec2 backMax(backMin.x + chip, backMin.y + chip);
	const ImVec2 frontMin(origin.x + inset, origin.y + inset);
	const ImVec2 frontMax(frontMin.x + chip, frontMin.y + chip);

	const bool frontActive = active != 1;
	drawChip(dl, backMin, backMax, back, !frontActive, backName);
	drawChip(dl, frontMin, frontMax, front, frontActive, frontName);

	const float swapR = std::max(5.f, chip * 0.18f);
	const ImVec2 swapC(origin.x + w - swapR - 1.f, origin.y + swapR + 1.f);
	dl->AddCircleFilled(swapC, swapR, IM_COL32(45, 48, 55, 220), 12);
	dl->AddCircle(swapC, swapR, IM_COL32(180, 185, 195, 255), 12, 1.f);
	{
		ImFont* font = ImGui::GetFont();
		const float glyphSz = swapR * 1.35f;
		const ImVec2 ts = font->CalcTextSizeA(glyphSz, FLT_MAX, 0.f, ICON_FA_EXCHANGE_ALT);
		dl->AddText(font, glyphSz, ImVec2(swapC.x - ts.x * 0.5f, swapC.y - ts.y * 0.5f),
					IM_COL32(220, 220, 225, 255), ICON_FA_EXCHANGE_ALT);
	}
	const ImVec2 swapMin(swapC.x - swapR, swapC.y - swapR);
	const ImVec2 swapMax(swapC.x + swapR, swapC.y + swapR);

	ImGuiStorage* store = ImGui::GetStateStorage();
	const ImGuiID slotId = ImGui::GetID("##pickerSlot");

	if (ImGui::IsMouseHoveringRect(swapMin, swapMax)) {
		ImGui::SetTooltip("Swap %s / %s", frontName, backName);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			// Exchange the colours. The active chip stays put, so the colour
			// you are painting with is the one that just moved into it.
			std::swap(front, back);
			edit.swapped = true;
		}
	} else if (chipClicked(frontMin, frontMax)) {
		active = 0;
	} else if (chipClicked(backMin, backMax)) {
		active = 1;
	}

	if (opts.enablePicker) {
		if (chipDoubleClicked(frontMin, frontMax)) {
			active = 0;
			store->SetInt(slotId, 0);
			ImGui::OpenPopup("##dualChipPicker");
		} else if (chipDoubleClicked(backMin, backMax)) {
			active = 1;
			store->SetInt(slotId, 1);
			ImGui::OpenPopup("##dualChipPicker");
		}

		if (ImGui::BeginPopup("##dualChipPicker")) {
			const int slot = store->GetInt(slotId, active == 1 ? 1 : 0);
			const bool isFront = slot != 1;
			ImGui::TextUnformatted(isFront ? frontName : backName);
			glm::vec4& c = isFront ? front : back;
			if (ImGui::ColorPicker4("##pick", &c.x,
									ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB |
										ImGuiColorEditFlags_InputRGB)) {
				active = slot;
				edit.picked = slot;
			}
			ImGui::EndPopup();
		}
	}

	ImGui::PopID();
	return edit;
}

} // namespace rigkit
