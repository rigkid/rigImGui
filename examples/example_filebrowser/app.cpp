#include "app.h"

#include "core/pack/MPack.h"
#include "core/RigKitEngine.h"
#include "packs/rigImGui/src/Mui.h"
#include "packs/rigImGui/src/rigImGui.h"

#include <spdlog/spdlog.h>

void FileBrowserApp::DemoWindow::renderContents() {
	if (ImGui::Button("Open Dialog")) {
		m_app->openDialog();
	}
	if (!m_app->lastPath().empty()) {
		ImGui::TextWrapped("Last: %s", m_app->lastPath().c_str());
	} else {
		ImGui::TextUnformatted("Pick a file - dialog opens on launch.");
	}
}

FileBrowserApp::FileBrowserApp() {
	window().width = 900;
	window().height = 580;
	window().title = "rigImGui - example_filebrowser";
	settings().appName = "example_filebrowser";
}

void FileBrowserApp::openDialog() {
	if (auto* mui = dynamic_cast<rigkit::Mui*>(m_engine->getUiManager())) {
		mui->openFileDialog("Select a File", {".*", ".png", ".json"},
							[this](const std::string& path) {
								setLastPath(path);
								spdlog::info("User chose: {}", path);
							});
	}
}

void FileBrowserApp::setup() {
	spdlog::info("example_filebrowser - Mui openFileDialog");
	m_engine->setClearColor(0.10f, 0.10f, 0.12f, 1.0f);

	auto* packs = m_engine->getPackManager();
	if (!packs) {
		return;
	}
	packs->registerPack<rigkit::rigImGui>();
	packs->initAll();
	packs->setupAll();

	if (auto* mui = dynamic_cast<rigkit::Mui*>(m_engine->getUiManager())) {
		mui->uiPrefs().showStatusBar = false;
		mui->setWindowVisibilityAll(false);
		if (auto* wm = mui->getWindowManager()) {
			auto demo = wm->createWindow<DemoWindow>(this);
			if (demo) {
				demo->setVisible(true);
			}
		}
	}
}

void FileBrowserApp::update(float dt) {
	if (m_opened) {
		return;
	}
	m_openDelay -= dt;
	if (m_openDelay > 0.f) {
		return;
	}
	m_opened = true;
	openDialog();
}
