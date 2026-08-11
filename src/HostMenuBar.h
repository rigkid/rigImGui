#pragma once

#include <string>

namespace rigkit {

class Mui;
class RigKitEngine;

/// Lean rigImGui host menu — App / File / Edit / View / Tools / Help.
class HostMenuBar {
  public:
	explicit HostMenuBar(Mui *ui);

	void setEngine(RigKitEngine *engine) { m_engine = engine; }
	void render();

  private:
	Mui *m_ui = nullptr;
	RigKitEngine *m_engine = nullptr;
	bool m_openWorkspaceSavePopup = false;
	char m_workspaceNameBuf[64] = {};

	void renderAppMenu();
	void renderFileMenu();
	void renderEditMenu();
	void renderViewMenu();
	void renderToolsMenu();
	void renderHelpMenu();
	void renderWorkspaceMenu();
	void renderWorkspaceSavePopup();
};

} // namespace rigkit
