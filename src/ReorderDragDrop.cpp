#include "ReorderDragDrop.h"

namespace rigkit {

IndexDropResult ReorderDragDropIndexRow(const char* payloadTag, int index, const char* previewLabel,
										float rowMinY, float rowMaxY) {
	IndexDropResult result;
	result.target = index;
	const float rowH = rowMaxY - rowMinY;

	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		ImGui::SetDragDropPayload(payloadTag, &index, sizeof(index));
		ImGui::TextUnformatted(previewLabel);
		ImGui::EndDragDropSource();
	}

	if (!ImGui::BeginDragDropTarget()) {
		return result;
	}

	const float mouseY = ImGui::GetIO().MousePos.y;
	const float zoneH = rowH / 3.0f;
	DropZone hoverZone = DropZone::After;
	if (mouseY < rowMinY + zoneH) {
		hoverZone = DropZone::Before;
	} else if (mouseY > rowMaxY - zoneH) {
		hoverZone = DropZone::After;
	} else {
		hoverZone = DropZone::After;
	}

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const float winL = ImGui::GetWindowPos().x;
	const float winR = winL + ImGui::GetWindowSize().x;
	const ImU32 kLine = IM_COL32(0, 190, 255, 255);

	const float lineY = (hoverZone == DropZone::Before) ? rowMinY : rowMaxY;
	dl->AddLine(ImVec2(winL, lineY), ImVec2(winR, lineY), kLine, 2.f);

	if (const ImGuiPayload* payload =
			ImGui::AcceptDragDropPayload(payloadTag, ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
		IM_ASSERT(payload->DataSize == sizeof(int));
		result.accepted = true;
		result.dragged = *static_cast<const int*>(payload->Data);
		result.zone = hoverZone;
	}

	ImGui::EndDragDropTarget();
	return result;
}

} // namespace rigkit
