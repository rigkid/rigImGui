#pragma once

#include "IWindow.h"

#include <string>

namespace rigkit {

class Mui;

/**
 * @brief Host panel for the shared MIDI mapper editor.
 * @details Contents are drawn by the ImMidiMapper owned by Mui (see Mui::midiMap());
 * this window only gives that editor a dockable host home. MIDI input, persistence
 * and output routing stay with the app - the host never opens a MIDI device.
 */
class MidiMapWindow : public IWindow {
  public:
	MidiMapWindow(const std::string& title, Mui* mui);

  protected:
	void renderContents() override;

  private:
	Mui* m_mui = nullptr;
};

} // namespace rigkit
