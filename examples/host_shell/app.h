#pragma once

#include <imgui.h>

#include "core/U_core.h"
#include "packs/rigImGui/src/IWindow.h"

class HostShellApp : public rigkit::IApp {
  public:
	class WidgetsWindow : public rigkit::IWindow {
	  public:
		WidgetsWindow() : IWindow("Widgets") {}
		void renderContents() override;

	  private:
		bool m_checked = true;
		float m_slider = 0.45f;
		int m_combo = 1;
		float m_color[4] = {0.26f, 0.59f, 0.98f, 1.f};
	};

	HostShellApp();
	void setup() override;
	void update(float) override {}
	void draw() override {}
};
