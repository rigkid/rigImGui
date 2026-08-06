#pragma once

#include <string>

#include "core/U_core.h"
#include "packs/rigImGui/src/IWindow.h"

class FileBrowserApp : public rigkit::IApp {
  public:
	class DemoWindow : public rigkit::IWindow {
	  public:
		explicit DemoWindow(FileBrowserApp* app)
			: IWindow("File Browser Demo"), m_app(app) {}
		void renderContents() override;

	  private:
		FileBrowserApp* m_app = nullptr;
	};

	FileBrowserApp();
	void setup() override;
	void update(float dt) override;
	void draw() override {}

	void openDialog();
	void setLastPath(std::string path) { m_lastPath = std::move(path); }
	const std::string& lastPath() const { return m_lastPath; }

  private:
	float m_openDelay = 0.35f;
	bool m_opened = false;
	std::string m_lastPath;
};
