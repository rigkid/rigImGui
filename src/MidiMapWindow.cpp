#include "MidiMapWindow.h"

#include "Mui.h"

#include <imgui.h>
#include "ImMidiMapper.h"

namespace rigkit {

MidiMapWindow::MidiMapWindow(const std::string& title, Mui* mui) : IWindow(title), m_mui(mui) {}

void MidiMapWindow::renderContents() {
	ImMidiMapper* mapper = m_mui ? m_mui->midiMap() : nullptr;
	if (!mapper) {
		ImGui::TextDisabled("MIDI mapper is not ready yet.");
		return;
	}
	mapper->drawEditorContents();
}

} // namespace rigkit
