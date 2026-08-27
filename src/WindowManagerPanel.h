#pragma once
#include <memory>
#include <string>
#include <vector>

#include "IWindow.h"
#include "MWindow.h"

namespace rigkit {

class WindowManagerPanel : public IWindow {
  public:
	WindowManagerPanel(const std::string &title, MWindow *windowManager,
					   ImGuiWindowFlags flags = 0);

  protected:
	void renderContents() override;

  private:
	MWindow *m_windowManager;
};

} // namespace rigkit
