#include "ShortcutsPanel.h"

#include "ShortcutManager.h"

#include <imgui.h>
#include <string>

namespace rigkit {
namespace {

bool isModifierKey(ImGuiKey key) {
	return key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl || key == ImGuiKey_LeftShift ||
		   key == ImGuiKey_RightShift || key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt ||
		   key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper ||
		   key == ImGuiKey_ReservedForModCtrl || key == ImGuiKey_ReservedForModShift ||
		   key == ImGuiKey_ReservedForModAlt || key == ImGuiKey_ReservedForModSuper;
}

bool tryCaptureChord(ShortcutChord& out) {
	ImGuiIO& io = ImGui::GetIO();
	for (ImGuiKey key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END;
		 key = static_cast<ImGuiKey>(key + 1)) {
		if (isModifierKey(key)) {
			continue;
		}
		// Mouse / gamepad stay out of keyboard remaps.
		if (key >= ImGuiKey_MouseLeft && key <= ImGuiKey_MouseWheelY) {
			continue;
		}
		if (key >= ImGuiKey_GamepadStart && key <= ImGuiKey_GamepadRStickRight) {
			continue;
		}
		if (!ImGui::IsKeyPressed(key, false)) {
			continue;
		}
		out = {key, io.KeyCtrl, io.KeyShift, io.KeyAlt};
		return true;
	}
	return false;
}

} // namespace

ShortcutsPanel::ShortcutsPanel(const std::string& title) : IWindow(title) {
	setCategory("Host");
}

void ShortcutsPanel::pollCapture() {
	if (!m_shortcuts || m_captureId.empty()) {
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		m_captureId.clear();
		m_shortcuts->setCapturing(false);
		return;
	}
	// Backspace / Delete clears the binding.
	if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) ||
		ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
		m_shortcuts->setChord(m_captureId, {});
		m_captureId.clear();
		m_shortcuts->setCapturing(false);
		return;
	}
	ShortcutChord chord;
	if (tryCaptureChord(chord)) {
		m_shortcuts->setChord(m_captureId, chord);
		m_captureId.clear();
		m_shortcuts->setCapturing(false);
	}
}

void ShortcutsPanel::renderContents() {
	if (!m_shortcuts) {
		ImGui::TextDisabled("No ShortcutManager");
		return;
	}

	pollCapture();

	ImGui::TextWrapped("Click a shortcut to remap. Esc cancels; Backspace clears.");
	if (ImGui::Button("Reset All")) {
		m_captureId.clear();
		m_shortcuts->setCapturing(false);
		m_shortcuts->resetAll();
	}
	ImGui::Separator();

	if (ImGui::BeginTable("shortcuts", 3,
						  ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
							  ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, 2.f);
		ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthStretch, 2.f);
		ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, 56.f);
		ImGui::TableHeadersRow();

		for (const auto& b : m_shortcuts->bindings()) {
			ImGui::PushID(b.id.c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(b.label.empty() ? b.id.c_str() : b.label.c_str());
			if (m_shortcuts->isCustom(b.id)) {
				ImGui::SameLine();
				ImGui::TextDisabled("*");
			}

			ImGui::TableSetColumnIndex(1);
			const bool capturing = (m_captureId == b.id);
			const std::string label =
				capturing ? std::string("Press key...") : shortcutChordLabel(b.chord());
			if (ImGui::Button(label.c_str(), ImVec2(-1.f, 0.f))) {
				if (capturing) {
					m_captureId.clear();
					m_shortcuts->setCapturing(false);
				} else {
					m_captureId = b.id;
					m_shortcuts->setCapturing(true);
				}
			}

			ImGui::TableSetColumnIndex(2);
			const bool canReset = m_shortcuts->isCustom(b.id);
			if (!canReset) {
				ImGui::BeginDisabled();
			}
			if (ImGui::SmallButton("Reset")) {
				m_shortcuts->resetChord(b.id);
				if (m_captureId == b.id) {
					m_captureId.clear();
					m_shortcuts->setCapturing(false);
				}
			}
			if (!canReset) {
				ImGui::EndDisabled();
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (!m_captureId.empty()) {
		ImGui::Spacing();
		ImGui::TextDisabled("Listening for %s", m_captureId.c_str());
	}
}

} // namespace rigkit
