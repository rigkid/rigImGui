#include "rigImGui.h"

#include "core/pack/MPack.h"
#include "core/pack/PackRegistry.h"
#include "core/RigKitEngine.h"
#include "core/util/MSettings.h"

#include <memory>
#include <spdlog/spdlog.h>

namespace rigkit {

rigImGui::rigImGui() : IPack("rigImGui") {}

bool rigImGui::init() {
	spdlog::info("[rigImGui] Initializing");

	auto* engine = getEngine();
	if (!engine) {
		spdlog::error("[rigImGui] No engine instance available");
		return false;
	}

	engine->registerUiChrome("imgui", [this]() {
		auto ui = std::make_unique<Mui>();
		m_ui = ui.get();
		return ui;
	});

	if (engine->getUiManager()) {
		spdlog::info("[rigImGui] UI manager already present - skipping creation");
		return true;
	}

	if (engine->uiChrome() != "imgui") {
		spdlog::info("[rigImGui] chrome='{}' - idle until swap", engine->uiChrome());
		return true;
	}

	auto uiPtr = std::make_unique<Mui>();
	m_ui = uiPtr.get();
	engine->attachUiManager(std::move(uiPtr));

	m_ui->init();
	engine->setUiInitialised(true);

	spdlog::info("[rigImGui] Ready - not required by SUDE");

	return true;
}

void rigImGui::setup() {
	auto* engine = getEngine();
	if (!engine || !m_ui) {
		return;
	}
	if (auto* settings = engine->getSettingsManager()) {
		settings->registerPreferences("rigImGui.ui", "Interface", &m_ui->uiPrefs(), [this]() {
			if (m_ui) {
				m_ui->applyUiPrefs();
			}
		});
		// Applying loaded prefs via onChanged should not look like a user edit.
		settings->clearDirty();
	}
}

void rigImGui::update(float /*dt*/) {}

void rigImGui::onUiAttached() {
	auto* engine = getEngine();
	if (!engine || engine->uiChrome() != "imgui") {
		m_ui = nullptr;
	}
}

void rigImGui::cleanup() {
	if (auto* engine = getEngine()) {
		if (auto* settings = engine->getSettingsManager()) {
			settings->unregisterPreferences("rigImGui.ui");
		}
	}
	m_ui = nullptr;
}

Mui* authorMui(RigKitEngine& engine) {
	if (engine.uiChrome() != "imgui") {
		return nullptr;
	}
	auto* packs = engine.getPackManager();
	if (!packs) {
		return nullptr;
	}
	auto pack = packs->getPack<rigImGui>();
	return pack ? pack->mui() : nullptr;
}

} // namespace rigkit

namespace {
struct rigImGuiRegistrar {
	rigImGuiRegistrar() {
		rigkit::PackRegistry::instance().addFactory("rigImGui", []() {
			return std::shared_ptr<rigkit::IPack>(std::make_shared<rigkit::rigImGui>());
		});
	}
};
static rigImGuiRegistrar rigImGui_auto_reg;
} // namespace
