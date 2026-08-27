#include "ChainEditor.h"

#include "ReorderDragDrop.h"

#include <algorithm>
#include <imgui.h>

#if __has_include("IconsFontAwesome5.h")
#include "IconsFontAwesome5.h"
#define RIG_CHAIN_GRIP ICON_FA_GRIP_LINES
#else
#define RIG_CHAIN_GRIP "##"
#endif

namespace rigkit {

void ChainEditor::draw() {
	ImGui::PushID(m_payloadTag);

	const auto drawList = [&]() {
		int removeIdx = -1;

		for (int i = 0; i < m_count; ++i) {
			ImGui::PushID(i);

			const std::string label = m_label ? m_label(i) : ("Step " + std::to_string(i));
			bool enabled = m_isEnabled ? m_isEnabled(i) : true;

			ImGui::AlignTextToFramePadding();
			if (m_showDragHandle) {
				ImGui::TextDisabled("%s", RIG_CHAIN_GRIP);
				ImGui::SameLine();
			}

			if (m_setEnabled) {
				if (ImGui::Checkbox("##en", &enabled)) {
					m_setEnabled(i, enabled);
				}
				ImGui::SameLine();
			}

			ImGuiTreeNodeFlags flags =
				m_defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
			bool visible = true;
			const bool open = m_onRemove ? ImGui::CollapsingHeader(label.c_str(), &visible, flags)
										 : ImGui::CollapsingHeader(label.c_str(), flags);
			// Close button sets visible=false but may still report open=true for
			// this frame - do not draw/edit the step that is about to be removed.
			const bool closing = m_onRemove && !visible;
			if (closing) {
				removeIdx = i;
			}

			if (!closing) {
				// Use the header item rect so drop hit-testing matches the bar
				// (cursor-based bounds drift when a grip/checkbox shares the row).
				const ImVec2 itemMin = ImGui::GetItemRectMin();
				const ImVec2 itemMax = ImGui::GetItemRectMax();
				auto drop =
					ReorderDragDropIndexRow(m_payloadTag, i, label.c_str(), itemMin.y, itemMax.y + 2.f);
				if (drop.accepted && drop.dragged != drop.target && m_onMove) {
					int insert = drop.target;
					if (drop.zone == DropZone::After) {
						insert = drop.target + 1;
					}
					if (drop.dragged < insert) {
						--insert;
					}
					m_onMove(drop.dragged, insert);
				}

				// Skip step bodies while a reorder drag is active.
				if (open && m_drawStep && ImGui::GetDragDropPayload() == nullptr) {
					ImGui::Indent();
					m_drawStep(i);
					ImGui::Unindent();
				}
			}

			ImGui::PopID();
		}

		// Remove after the list walk so indices stay stable for this frame.
		if (removeIdx >= 0 && m_onRemove) {
			m_onRemove(removeIdx);
		}
	};

	const auto drawAddRow = [&]() {
		if (!m_showAddRow || m_addTypes.empty() || !m_onAdd) {
			return;
		}

		m_addIndex = std::clamp(m_addIndex, 0, (int)m_addTypes.size() - 1);
		std::vector<const char*> labels;
		labels.reserve(m_addTypes.size());
		for (const auto& s : m_addTypes) {
			labels.push_back(s.c_str());
		}

		if (m_addOnSelect) {
			ImGui::SetNextItemWidth(std::max(48.f, ImGui::GetContentRegionAvail().x));
			if (ImGui::Combo("##addtype", &m_addIndex, labels.data(), (int)labels.size()) &&
				m_addIndex > 0) {
				m_onAdd(m_addIndex);
				m_addIndex = 0;
			}
			return;
		}

		const ImGuiStyle& st = ImGui::GetStyle();
		const float buttonW =
			ImGui::CalcTextSize(m_addButtonLabel).x + st.FramePadding.x * 2.f;
		const float spacing = st.ItemSpacing.x;
		ImGui::SetNextItemWidth(
			std::max(48.f, ImGui::GetContentRegionAvail().x - buttonW - spacing));
		ImGui::Combo("##addtype", &m_addIndex, labels.data(), (int)labels.size());
		ImGui::SameLine(0.f, spacing);
		if (ImGui::Button(m_addButtonLabel, ImVec2(buttonW, 0.f)) && m_addIndex > 0) {
			m_onAdd(m_addIndex);
		}
	};

	if (!m_sectionTitle.empty()) {
		const std::string hdr = m_sectionTitle + " (" + std::to_string(m_count) + ")";
		if (ImGui::CollapsingHeader(hdr.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			drawList();
			drawAddRow();

			if (!m_footerHint.empty()) {
				ImGui::TextDisabled("%s", m_footerHint.c_str());
			}

			if (m_sectionFooter) {
				m_sectionFooter();
			}
		}
	} else {
		drawList();
		drawAddRow();

		if (!m_footerHint.empty()) {
			ImGui::TextDisabled("%s", m_footerHint.c_str());
		}
	}

	ImGui::PopID();
}

} // namespace rigkit
