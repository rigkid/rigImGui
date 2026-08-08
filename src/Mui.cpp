#include "Mui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <spdlog/spdlog.h>
#include "DebugPanel.h"
#include "ExportPng.h"
#include "Handle2D.h"
#include "ImGuiStyleKit.h"
#include "LayersWindow.h"
#include "LogWindow.h"
#include "PreferencesPanel.h"
#include "PropertiesWindow.h"
#include "Rulers.h"
#include "SceneWindow.h"
#include "ShortcutsPanel.h"
#include "ThemePanel.h"
#include "UiDpi.h"
#include "ViewportWindow.h"
#include "WindowManagerPanel.h"
#include "core/RigKitEngine.h"
#include "core/pack/IPack.h"
#include "core/pack/MPack.h"
#include "core/util/AppPaths.h"
#include "core/util/MSettings.h"
#include "core/util/UndoStack.h"
#include "ecs/systems/SEvent.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "rendering/U_gladGlfw.h"

namespace {

std::string resolveThemePath(const std::string& path) {
	if (path.empty()) {
		return {};
	}
	namespace fs = std::filesystem;
	fs::path p(path);
	if (p.is_absolute()) {
		return p.lexically_normal().string();
	}
	return (fs::path(AppPaths::getThemesDir()) / p).lexically_normal().string();
}

} // namespace

namespace rigkit {

Mui::Mui() {
	m_windowManager = std::make_unique<MWindow>();
	m_menuBar = std::make_unique<HostMenuBar>(this);
}

Mui::~Mui() { shutdown(); }

void Mui::setRigKitEngine(RigKitEngine *engine) {
	m_engine = engine;
	if (m_windowManager) {
		m_windowManager->setEngine(engine);
	}
	if (m_menuBar) {
		m_menuBar->setEngine(engine);
	}
	if (engine) {
		m_window = engine->getWindow();
		loadRecentFilesFromSettings();
		// Restore the active workspace name (menu checkmark); the layout
		// itself lives in the autosaved session imgui.ini.
		if (auto *settings = engine->getSettingsManager()) {
			const json ws = settings->getValue("workspace");
			if (ws.is_string()) {
				m_currentWorkspace = ws.get<std::string>();
			}
		}
	}
}

void Mui::init() {
	if (!m_imguiReady) {
		initImGui();
	}
}

void Mui::shutdown() {
	if (m_imguiReady) {
		shutdownImGui();
	}
}

void Mui::initImGui() {
	if (!m_window) {
		if (m_engine) {
			m_window = m_engine->getWindow();
		}
	}
	if (!m_window) {
		spdlog::error("[rigImGui] No GLFW window for ImGui init");
		return;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Persist layout next to the exe (not cwd) so docks survive relaunches.
	{
		namespace fs = std::filesystem;
		const fs::path iniPath(AppPaths::getUiIniPath());
		std::error_code ec;
		fs::create_directories(iniPath.parent_path(), ec);
		m_iniPath = iniPath.string();
		io.IniFilename = m_iniPath.c_str();
	}

	// Backends first — newer ImGui sets RendererHasTextures before atlas work.
	ImGui_ImplGlfw_InitForOpenGL(m_window, true);
#if defined(RIGKIT_GLES)
	// Pi native GLES2 or desktop ANGLE — GLSL ES 1.00.
	ImGui_ImplOpenGL3_Init("#version 100");
#else
	ImGui_ImplOpenGL3_Init("#version 130");
#endif

	m_dpiScale = ImGui_ImplGlfw_GetContentScaleForWindow(m_window);
	if (m_dpiScale < 0.5f) {
		m_dpiScale = 1.f;
	}
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 3
	io.ConfigDpiScaleFonts = true;
	io.ConfigDpiScaleViewports = true;
#endif

	// Fonts then theme (prefs may re-apply in pack setup after registerPreferences).
	ImGuiStyleKit::loadFonts(io, AppPaths::getFontsDir(), m_uiPrefs.fontFile, m_uiPrefs.fontSize);
	for (const auto& hook : m_fontAtlasHooks) {
		if (hook) {
			hook(*io.Fonts);
		}
	}
	m_appliedFontFile = m_uiPrefs.fontFile;
	m_appliedFontSize = m_uiPrefs.fontSize;
	setImGuiTheme(m_currentTheme);

	m_imguiReady = true;
	spdlog::info("[rigImGui] ImGui context ready (dpi scale {:.2f})", m_dpiScale);
}

void Mui::shutdownImGui() {
	if (!m_imguiReady) {
		return;
	}
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	m_imguiReady = false;
}

void Mui::handleInput() {
	// Named Kit shortcuts use ImGui key state, which is only valid after
	// NewFrame — see render(). GLFW still feeds ImGui via callbacks.
}

void Mui::setupDockspace() {
	// Host menu is BeginMainMenuBar() — do not also reserve a window MenuBar here
	// (that left an empty strip under the real menu on every app).
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	m_statusBarHeight = statusBarVisible() ? ImGui::GetFrameHeight() : 0.f;
	ImVec2 dockPos = viewport->WorkPos;
	ImVec2 dockSize = viewport->WorkSize;
	if (m_statusBarHeight > 0.f && dockSize.y > m_statusBarHeight) {
		dockSize.y -= m_statusBarHeight;
	}
	ImGui::SetNextWindowPos(dockPos);
	ImGui::SetNextWindowSize(dockSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
					ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
					ImGuiWindowFlags_NoBringToFrontOnFocus |
					ImGuiWindowFlags_NoNavFocus;
	// Passthru central: DockSpace paints the hole; Begin must not fill over the GL bed.
	if (m_dockPassthroughCentral) {
		window_flags |= ImGuiWindowFlags_NoBackground;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace", nullptr, window_flags);
	ImGui::PopStyleVar(3);

	ImGuiID dockspace_id = ImGui::GetID("RigDockSpace");
	m_dockspaceId = dockspace_id;
	if (m_dockLayoutBuilder) {
		m_dockLayoutBuilder(dockspace_id);
	}
	ImGuiDockNodeFlags dock_flags = ImGuiDockNodeFlags_None;
	if (m_dockPassthroughCentral) {
		// Empty center shows the GL bed; block panels from docking over it.
		dock_flags |= ImGuiDockNodeFlags_PassthruCentralNode |
					  ImGuiDockNodeFlags_NoDockingOverCentralNode;
	}
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dock_flags);
	m_centralValid = false;
	if (const ImGuiDockNode* central = ImGui::DockBuilderGetCentralNode(dockspace_id)) {
		if (central->Size.x > 1.f && central->Size.y > 1.f) {
			m_centralX = central->Pos.x;
			m_centralY = central->Pos.y;
			m_centralW = central->Size.x;
			m_centralH = central->Size.y;
			m_centralValid = true;
		}
	}
	ImGui::End();
}

void Mui::renderAllWindows() {
	// Opted in + outside Edit Mode → skip panels (clean canvas). Visibility flags untouched.
	if (editModeEnabled() && !m_editMode) {
		return;
	}
	if (m_windowManager) {
		m_windowManager->renderAllWindows();
	}
}

void Mui::tickNotifications(float dt) {
	for (auto it = m_notifications.begin(); it != m_notifications.end();) {
		it->timeRemaining -= dt;
		if (it->timeRemaining <= 0.f) {
			it = m_notifications.erase(it);
		} else {
			++it;
		}
	}
}

void Mui::renderNotifications() {
	if (m_notifications.empty()) {
		return;
	}

	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	const float pad = uiPx(28.f);
	float prefW = m_uiPrefs.notificationWidth;
	if (prefW < 160.f) {
		prefW = 160.f;
	}
	if (prefW > 600.f) {
		prefW = 600.f;
	}
	const float maxW =
		std::min(uiPx(prefW), std::max(1.f, viewport->WorkSize.x - 2.f * pad));
	const float maxH = std::max(1.f, viewport->WorkSize.y - 2.f * pad);

	const ImVec2 anchor(viewport->WorkPos.x + viewport->WorkSize.x - pad,
						viewport->WorkPos.y + pad);
	ImGui::SetNextWindowPos(anchor, ImGuiCond_Always, ImVec2(1.f, 0.f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(0.f, 0.f), ImVec2(maxW, maxH));
	ImGui::SetNextWindowBgAlpha(0.85f);
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoFocusOnAppearing;

	if (ImGui::Begin("##rigImGuiNotifications", nullptr, flags)) {
		ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + maxW);
		for (const auto &n : m_notifications) {
			ImVec4 color(1.f, 1.f, 1.f, 1.f);
			switch (n.type) {
			case NotificationType::Success:
				color = ImVec4(0.4f, 0.9f, 0.5f, 1.f);
				break;
			case NotificationType::Warning:
				color = ImVec4(1.f, 0.85f, 0.3f, 1.f);
				break;
			case NotificationType::Error:
				color = ImVec4(1.f, 0.4f, 0.4f, 1.f);
				break;
			default:
				break;
			}
			ImGui::TextColored(color, "%s", n.message.c_str());
		}
		ImGui::PopTextWrapPos();
	}
	ImGui::End();
}

void Mui::renderModals() {
	for (auto it = m_modals.begin(); it != m_modals.end();) {
		if (!it->open) {
			it = m_modals.erase(it);
			continue;
		}

		ImGui::OpenPopup(it->title.c_str());
		bool open = it->open;
		if (ImGui::BeginPopupModal(it->title.c_str(), &open,
								   ImGuiWindowFlags_AlwaysAutoResize)) {
			// Explicit wrap width — TextWrapped + AlwaysAutoResize otherwise stays tiny.
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 400.f);
			ImGui::TextUnformatted(it->message.c_str());
			ImGui::PopTextWrapPos();
			ImGui::Separator();
			if (ImGui::Button("OK", ImVec2(120, 0))) {
				if (it->onOk) {
					it->onOk();
				}
				open = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		it->open = open;
		if (!it->open) {
			it = m_modals.erase(it);
		} else {
			++it;
		}
	}
}

void Mui::renderAbout() {
	if (!m_aboutOpen) {
		return;
	}

	ImGui::OpenPopup("About");
	ImGui::SetNextWindowSize(ImVec2(460.f, 520.f), ImGuiCond_Appearing);
	bool open = m_aboutOpen;
	if (ImGui::BeginPopupModal("About", &open, 0)) {
		ImGui::TextUnformatted("RigKit");
		ImGui::Spacing();
		ImGui::TextWrapped(
			"Rig you can code. Setup, update, draw — shapes on screen "
			"without a manager tree. Plain data travels; packs bring systems, "
			"UI, and tools. Raspberry Pi is the floor; rebuilds stay cheap.");
		ImGui::Spacing();
		ImGui::TextDisabled("MIT Rigkid Contributors");
		ImGui::Separator();
		ImGui::TextUnformatted("Loaded packs");
		const float footer = ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("about_packs", ImVec2(0.f, -footer),
							  ImGuiChildFlags_Borders)) {
			if (m_engine && m_engine->getPackManager()) {
				auto packs = m_engine->getPackManager()->getAllPacks();
				std::sort(packs.begin(), packs.end(),
						  [](const std::shared_ptr<IPack> &a,
							 const std::shared_ptr<IPack> &b) {
							  return a->getName() < b->getName();
						  });
				for (const auto &pack : packs) {
					if (!pack) {
						continue;
					}
					ImGui::PushID(pack->getName().c_str());
					if (ImGui::CollapsingHeader(pack->getName().c_str())) {
						ImGui::Indent();
						const auto &desc = pack->getDescription();
						if (!desc.empty()) {
							ImGui::TextWrapped("%s", desc.c_str());
							ImGui::Spacing();
						}
						ImGui::TextWrapped("%s", pack->getLicense().c_str());
						ImGui::Unindent();
					}
					ImGui::PopID();
				}
				if (packs.empty()) {
					ImGui::TextDisabled("No packs loaded");
				}
			} else {
				ImGui::TextDisabled("Pack manager unavailable");
			}
		}
		ImGui::EndChild();
		if (ImGui::Button("OK", ImVec2(120.f, 0.f))) {
			open = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	m_aboutOpen = open;
}

void Mui::render() {
	if (!m_imguiReady) {
		return;
	}

	// Font atlas Clear() must not run between NewFrame and Render.
	if (m_pendingFontReload) {
		m_pendingFontReload = false;
		reloadFonts();
	}

	// Workspace ini swap must also stay outside the frame.
	if (!m_pendingWorkspaceLoad.empty()) {
		ImGui::LoadIniSettingsFromDisk(m_pendingWorkspaceLoad.c_str());
		m_pendingWorkspaceLoad.clear();
	}

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	m_inImGuiFrame = true;

	installDefaultShortcuts();
	m_shortcuts.handleInput();
	ensureDefaultStatusSlots();

	if (m_engine) {
		const float dt = m_engine->getDeltaTime();
		tickNotifications(dt);
		m_progress.tickFrame(dt);
	}

	if (m_menuBar) {
		m_menuBar->render();
	}
	setupDockspace();
	renderAllWindows();
	renderMainViewOverlays();
	if (m_gizmoDrawer && m_engine) {
		float gx = 0.f, gy = 0.f, gw = 0.f, gh = 0.f;
		if (!centralViewRect(gx, gy, gw, gh)) {
			gw = static_cast<float>(m_engine->getWindowWidth());
			gh = static_cast<float>(m_engine->getWindowHeight());
		}
		m_gizmoDrawer(gx, gy, gw, gh, m_gizmoOp);
	}
	renderStatusBar();
	renderProgressFloating();
	renderNotifications();
	renderModals();
	renderAbout();
	m_fileDialogs.tick();

	ImGui::Render();
	m_inImGuiFrame = false;
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	flushExportPng();
}

void Mui::setWindowVisibility(const std::string &windowName, bool visible) {
	if (m_windowManager) {
		m_windowManager->setWindowVisible(windowName, visible);
	}
}

void Mui::setWindowVisibilityAll(bool visible) {
	if (!m_windowManager) {
		return;
	}
	if (visible) {
		m_windowManager->showAllWindows();
	} else {
		m_windowManager->hideAllWindows();
	}
}

bool Mui::getWindowVisibility(const std::string &windowName) const {
	return m_windowManager ? m_windowManager->getWindowVisibility(windowName)
						   : false;
}

std::vector<std::string> Mui::getAllWindowNames() const {
	return m_windowManager ? m_windowManager->getAllWindowNames()
						   : std::vector<std::string>{};
}

MWindow *Mui::getWindowManager() { return m_windowManager.get(); }

const MWindow *Mui::getWindowManager() const { return m_windowManager.get(); }

bool Mui::centralViewRect(float& outX, float& outY, float& outW, float& outH) const {
	if (!m_centralValid) {
		outX = outY = 0.f;
		outW = outH = 0.f;
		return false;
	}
	outX = m_centralX;
	outY = m_centralY;
	outW = m_centralW;
	outH = m_centralH;
	return true;
}

void Mui::applyDpiStyle() {
	// ScaleAllSizes is multiplicative and lossy. Prefer a relative step from
	// style._MainScale so applyTheme (colors-only) + applyUiPrefs does not
	// compound WindowPadding on high-DPI (e.g. 2.25x → 5x).
	ImGuiStyle& style = ImGui::GetStyle();
	style.FontScaleDpi = m_dpiScale;
	const float cur = (style._MainScale > 0.01f) ? style._MainScale : 1.f;
	const float factor = m_dpiScale / cur;
	if (factor > 1.01f || factor < 0.99f) {
		style.ScaleAllSizes(factor);
	}
}

void Mui::setImGuiTheme(ImGuiTheme theme) {
	theme = clampImGuiTheme(static_cast<int>(theme));
	const bool changed = m_currentTheme != theme || m_uiPrefs.theme != static_cast<int>(theme);
	m_currentTheme = theme;
	m_uiPrefs.theme = static_cast<int>(theme);
	ImGuiStyleKit::applyTheme(theme);
	applyDpiStyle();
	if (changed && m_engine) {
		if (auto* settings = m_engine->getSettingsManager()) {
			settings->markDirty();
		}
	}
}

bool Mui::saveCurrentTheme(const std::string& path, bool notify) {
	const std::string resolved = resolveThemePath(path.empty() ? "custom.json" : path);
	const bool ok = ImGuiStyleKit::saveStyleToFile(resolved, ImGui::GetStyle(),
												   static_cast<int>(m_currentTheme));
	if (ok) {
		// Store relative name when under themes dir for portable prefs.
		namespace fs = std::filesystem;
		const fs::path themes(AppPaths::getThemesDir());
		const fs::path full(resolved);
		std::error_code ec;
		const fs::path rel = fs::relative(full, themes, ec);
		const std::string relStr = rel.generic_string();
		m_uiPrefs.themeFile =
			(!ec && !relStr.empty() && relStr.find("..") == std::string::npos) ? relStr : resolved;
		if (m_engine) {
			if (auto* settings = m_engine->getSettingsManager()) {
				settings->markDirty();
			}
		}
		if (notify) {
			showNotification("Theme saved: " + m_uiPrefs.themeFile, NotificationType::Success);
		}
	} else if (notify) {
		showNotification("Failed to save theme", NotificationType::Error);
	}
	return ok;
}

bool Mui::loadTheme(const std::string& path, bool notify) {
	const std::string resolved = resolveThemePath(path.empty() ? m_uiPrefs.themeFile : path);
	if (resolved.empty()) {
		if (notify) {
			showNotification("No theme file specified", NotificationType::Warning);
		}
		return false;
	}
	int baseTheme = static_cast<int>(m_currentTheme);
	ImGuiStyle& style = ImGui::GetStyle();
	if (!ImGuiStyleKit::loadStyleFromFile(resolved, style, &baseTheme)) {
		if (notify) {
			showNotification("Failed to load theme", NotificationType::Error);
		}
		return false;
	}
	m_currentTheme = clampImGuiTheme(baseTheme);
	m_uiPrefs.theme = static_cast<int>(m_currentTheme);
	namespace fs = std::filesystem;
	const fs::path themes(AppPaths::getThemesDir());
	const fs::path full(resolved);
	std::error_code ec;
	const fs::path rel = fs::relative(full, themes, ec);
	const std::string relStr = rel.generic_string();
	m_uiPrefs.themeFile =
		(!ec && !relStr.empty() && relStr.find("..") == std::string::npos) ? relStr : resolved;
	// Themes are saved from the live style (already DPI-baked). Mark scale so
	// applyDpiStyle does not multiply padding again.
	style._MainScale = (m_dpiScale > 0.01f) ? m_dpiScale : 1.f;
	applyDpiStyle();
	if (m_engine) {
		if (auto* settings = m_engine->getSettingsManager()) {
			settings->markDirty();
		}
	}
	if (notify) {
		showNotification("Theme loaded: " + m_uiPrefs.themeFile, NotificationType::Success);
	}
	return true;
}

void Mui::applyUiPrefs() {
	// Theme path: applyTheme (unscaled metrics) → ScaleAllSizes once.
	// Do not ScaleAllSizes again in reloadFonts — that compounds padding on high DPI.
	setImGuiTheme(clampImGuiTheme(m_uiPrefs.theme));
	if (!m_uiPrefs.themeFile.empty()) {
		loadTheme(m_uiPrefs.themeFile, false);
	}
	const bool fontChanged =
		m_uiPrefs.fontFile != m_appliedFontFile || m_uiPrefs.fontSize != m_appliedFontSize;
	if (fontChanged) {
		reloadFonts();
	}
	syncProgressFromPrefs();
	syncFpsChromeFromPrefs();
}

void Mui::syncProgressFromPrefs() {
	const bool useBar = m_uiPrefs.showStatusBar && m_uiPrefs.progressUseStatusBar;
	m_progress.setUseStatusBar(useBar);
	m_progress.setAutoHideDelay(m_uiPrefs.progressAutoHideSeconds);
}

bool Mui::reloadFonts() {
	if (!m_imguiReady) {
		return false;
	}
	// Clearing the atlas mid-frame invalidates fonts in the open draw list.
	if (m_inImGuiFrame) {
		m_pendingFontReload = true;
		return true;
	}
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();
	const bool ok =
		ImGuiStyleKit::loadFonts(io, AppPaths::getFontsDir(), m_uiPrefs.fontFile, m_uiPrefs.fontSize);
	for (const auto& hook : m_fontAtlasHooks) {
		if (hook) {
			hook(*io.Fonts);
		}
	}
	if (ok) {
		m_appliedFontFile = m_uiPrefs.fontFile;
		m_appliedFontSize = m_uiPrefs.fontSize;
	}
	return ok;
}

void Mui::registerFontAtlasHook(std::function<void(ImFontAtlas& atlas)> hook) {
	if (hook) {
		m_fontAtlasHooks.push_back(std::move(hook));
	}
	// If ImGui is already up, rebuild so the new hook's fonts appear.
	if (m_imguiReady) {
		reloadFonts();
	}
}

namespace {
// Workspace names become filenames; keep them portable. "imgui" is the
// session autosave and stays reserved.
bool workspaceNameValid(const std::string &name) {
	if (name.empty() || name == "imgui") {
		return false;
	}
	for (char c : name) {
		if (c == '/' || c == '\\' || c == ':' || c == '.') {
			return false;
		}
	}
	return true;
}

std::filesystem::path workspaceIniPath(const std::string &name) {
	return std::filesystem::path(AppPaths::getWorkspacesDir()) / (name + ".ini");
}
} // namespace

bool Mui::saveWorkspace(const std::string &name) {
	if (!m_imguiReady || !workspaceNameValid(name)) {
		return false;
	}
	const std::filesystem::path path = workspaceIniPath(name);
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	ImGui::SaveIniSettingsToDisk(path.string().c_str());
	if (!std::filesystem::exists(path, ec)) {
		showNotification("Could not save workspace: " + name, NotificationType::Error);
		return false;
	}
	m_currentWorkspace = name;
	persistCurrentWorkspace();
	showNotification("Workspace saved: " + name, NotificationType::Success);
	return true;
}

bool Mui::loadWorkspace(const std::string &name) {
	if (!workspaceNameValid(name)) {
		return false;
	}
	const std::filesystem::path path = workspaceIniPath(name);
	std::error_code ec;
	if (!std::filesystem::exists(path, ec)) {
		showNotification("Workspace not found: " + name, NotificationType::Warning);
		return false;
	}
	// Swapping dock state mid-frame corrupts the open draw pass — defer to
	// the frame boundary in render(), same as font reloads.
	m_pendingWorkspaceLoad = path.string();
	m_currentWorkspace = name;
	persistCurrentWorkspace();
	return true;
}

bool Mui::deleteWorkspace(const std::string &name) {
	if (!workspaceNameValid(name)) {
		return false;
	}
	std::error_code ec;
	const bool removed = std::filesystem::remove(workspaceIniPath(name), ec);
	if (m_currentWorkspace == name) {
		m_currentWorkspace.clear();
		persistCurrentWorkspace();
	}
	return removed && !ec;
}

std::vector<std::string> Mui::workspaceNames() const {
	std::vector<std::string> names;
	std::error_code ec;
	std::filesystem::directory_iterator it(AppPaths::getWorkspacesDir(), ec);
	if (ec) {
		return names;
	}
	for (const auto &entry : it) {
		if (!entry.is_regular_file(ec) || entry.path().extension() != ".ini") {
			continue;
		}
		const std::string stem = entry.path().stem().string();
		if (stem != "imgui") {
			names.push_back(stem);
		}
	}
	std::sort(names.begin(), names.end());
	return names;
}

void Mui::persistCurrentWorkspace() {
	if (!m_engine) {
		return;
	}
	auto *settings = m_engine->getSettingsManager();
	if (!settings) {
		return;
	}
	settings->setValue("workspace", m_currentWorkspace);
	settings->saveToDisk();
}

const char* Mui::hostPanelTitle(HostPanel panel) {
	switch (panel) {
	case HostPanel::Log:
		return "Log";
	case HostPanel::Windows:
		return "Windows";
	case HostPanel::Debug:
		return "Debug";
	case HostPanel::Properties:
		return "Properties";
	case HostPanel::Scene:
		return "Scene";
	case HostPanel::Layers:
		return "Layers";
	case HostPanel::Viewport:
		return "Viewport";
	case HostPanel::Shortcuts:
		return "Shortcuts";
	case HostPanel::Theme:
		return "Theme";
	case HostPanel::Preferences:
		return "Preferences";
	}
	return "Host";
}

std::shared_ptr<IWindow> Mui::ensureHostPanel(HostPanel panel) {
	if (!m_windowManager) {
		return nullptr;
	}
	const char* title = hostPanelTitle(panel);
	if (auto existing = m_windowManager->getWindow<IWindow>(title)) {
		return existing;
	}

	std::shared_ptr<IWindow> window;
	switch (panel) {
	case HostPanel::Log: {
		auto log = m_windowManager->createWindow<LogWindow>(title, ImGuiWindowFlags_MenuBar);
		if (log) {
			log->setupSpdlogSink();
		}
		window = log;
		break;
	}
	case HostPanel::Windows:
		window = m_windowManager->createWindow<WindowManagerPanel>(title, m_windowManager.get());
		break;
	case HostPanel::Debug:
		window = m_windowManager->createWindow<DebugPanel>(title);
		break;
	case HostPanel::Properties:
		window = m_windowManager->createWindow<PropertiesWindow>(title);
		break;
	case HostPanel::Scene:
		window = m_windowManager->createWindow<SceneWindow>(title);
		break;
	case HostPanel::Layers:
		window = m_windowManager->createWindow<LayersWindow>(title);
		break;
	case HostPanel::Viewport:
		window = m_windowManager->createWindow<ViewportWindow>(title);
		break;
	case HostPanel::Shortcuts: {
		auto shortcuts = m_windowManager->createWindow<ShortcutsPanel>(title);
		if (shortcuts) {
			shortcuts->setShortcutManager(&m_shortcuts);
		}
		window = shortcuts;
		break;
	}
	case HostPanel::Theme:
		window = m_windowManager->createWindow<ThemePanel>(title);
		break;
	case HostPanel::Preferences:
		window = m_windowManager->createWindow<PreferencesPanel>(title);
		break;
	}

	if (window) {
		window->setCategory("Host");
	}
	return window;
}

void Mui::addHostPanel(HostPanel panel) {
	if (auto window = ensureHostPanel(panel)) {
		window->setVisible(true);
	}
}

void Mui::addHostPanels(std::initializer_list<HostPanel> panels) {
	for (HostPanel panel : panels) {
		addHostPanel(panel);
	}
}

void Mui::addAllHostPanels() {
	if (!m_windowManager) {
		return;
	}

	// Sketch defaults: visible Log / Windows / Properties / Scene / Layers;
	// others created but hidden (opt-in via View → Windows).
	const struct {
		HostPanel panel;
		bool visible;
	} defaults[] = {
		{HostPanel::Log, true},
		{HostPanel::Windows, true},
		{HostPanel::Debug, false},
		{HostPanel::Properties, true},
		{HostPanel::Scene, true},
		{HostPanel::Layers, true},
		{HostPanel::Viewport, false},
		{HostPanel::Shortcuts, false},
		{HostPanel::Theme, false},
		{HostPanel::Preferences, false},
	};
	for (const auto& row : defaults) {
		if (auto window = ensureHostPanel(row.panel)) {
			window->setVisible(row.visible);
		}
	}

	if (!m_dockLayoutBuilder) {
		setFirstRunHostDockLayout();
	}
	spdlog::info("[rigImGui] Host panels ready (Log, Windows, Debug, Properties, "
				 "Scene, Layers, Viewport, Shortcuts, Theme, Preferences)");
}

void Mui::setFirstRunHostDockLayout(std::vector<std::string> extraRightWindows) {
	m_dockLayoutBuilder = [this, extras = std::move(extraRightWindows)](ImGuiID dockspaceId) {
		ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
		if (!node) {
			return; // DockSpace not created yet
		}
		if (node->IsSplitNode()) {
			// Restored from imgui.ini — leave user layout alone.
			setDockLayoutBuilder(nullptr);
			return;
		}

		const ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, vp->WorkSize);

		ImGuiID left = 0, center = 0, right = 0, bottom = 0;
		ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.22f, &left, &center);
		ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, &right, &center);
		ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, &bottom, &center);

		ImGui::DockBuilderDockWindow(hostPanelTitle(HostPanel::Scene), left);
		ImGui::DockBuilderDockWindow(hostPanelTitle(HostPanel::Layers), left);
		ImGui::DockBuilderDockWindow(hostPanelTitle(HostPanel::Windows), left);
		ImGui::DockBuilderDockWindow(hostPanelTitle(HostPanel::Properties), right);
		for (const auto& title : extras) {
			ImGui::DockBuilderDockWindow(title.c_str(), right);
		}
		ImGui::DockBuilderDockWindow(hostPanelTitle(HostPanel::Log), bottom);
		// Leave `center` empty — GL clear / install present (or Viewport if shown).
		ImGui::DockBuilderFinish(dockspaceId);
		setDockLayoutBuilder(nullptr);
	};
}

void Mui::showPreferences() {
	addHostPanel(HostPanel::Preferences);
}

void Mui::showNotification(const std::string &message, NotificationType type,
						   float duration) {
	float seconds = duration;
	if (seconds < 0.f) {
		seconds = m_uiPrefs.notificationSeconds;
	}
	if (seconds < 0.5f) {
		seconds = 0.5f;
	}
	m_notifications.push_back({message, type, seconds});
}

void Mui::showModal(const std::string &title, const std::string &message,
					NotificationType type, std::function<void()> onOk) {
	m_modals.push_back({title, message, type, std::move(onOk), true});
}

void Mui::showAbout() {
	m_aboutOpen = true;
}

namespace {
constexpr const char *kEditModeStatus = "Edit Mode";
}

bool Mui::editModeEnabled() const {
	return m_engine && m_engine->editModeEnabled();
}

void Mui::setEditMode(bool on) {
	if (!editModeEnabled()) {
		return;
	}
	m_editMode = on;
}

void Mui::openFileDialog(const std::string &title, std::vector<std::string> filters,
						 FileDialogCallback onSelected) {
	m_fileDialogs.open(title, std::move(filters), std::move(onSelected));
}

void Mui::saveFileDialog(const std::string &title, std::vector<std::string> filters,
						 FileDialogCallback onSelected) {
	m_fileDialogs.save(title, std::move(filters), std::move(onSelected));
}

void Mui::registerFileAction(const std::string &label, std::function<void()> action,
							 const std::string &shortcut) {
	m_fileActions.push_back(FileMenuAction{label, shortcut, std::move(action), false});
}

void Mui::registerFileSubmenu(const std::string &label, std::function<void()> drawContents) {
	m_fileActions.push_back(FileMenuAction{label, {}, std::move(drawContents), true});
}

void Mui::loadRecentFilesFromSettings() {
	m_recentFiles.clear();
	if (!m_engine) {
		return;
	}
	auto *settings = m_engine->getSettingsManager();
	if (!settings) {
		return;
	}
	const json j = settings->getValue("recentFiles");
	if (!j.is_array()) {
		return;
	}
	for (const auto &entry : j) {
		if (!entry.is_string()) {
			continue;
		}
		const std::string path = entry.get<std::string>();
		if (path.empty()) {
			continue;
		}
		m_recentFiles.push_back(path);
		if (static_cast<int>(m_recentFiles.size()) >= kMaxRecentFiles) {
			break;
		}
	}
}

void Mui::persistRecentFiles() {
	if (!m_engine) {
		return;
	}
	auto *settings = m_engine->getSettingsManager();
	if (!settings) {
		return;
	}
	json arr = json::array();
	for (const auto &path : m_recentFiles) {
		arr.push_back(path);
	}
	settings->setValue("recentFiles", arr);
	settings->saveToDisk();
}

void Mui::noteRecentFile(const std::string &path) {
	if (path.empty()) {
		return;
	}
	std::error_code ec;
	std::filesystem::path fp(path);
	const std::string stored =
		std::filesystem::weakly_canonical(fp, ec).string();
	const std::string key = ec ? path : stored;
	m_recentFiles.erase(std::remove(m_recentFiles.begin(), m_recentFiles.end(), key),
						m_recentFiles.end());
	// Also drop any non-canonical duplicate of the same path.
	if (key != path) {
		m_recentFiles.erase(std::remove(m_recentFiles.begin(), m_recentFiles.end(), path),
							m_recentFiles.end());
	}
	m_recentFiles.insert(m_recentFiles.begin(), key);
	if (static_cast<int>(m_recentFiles.size()) > kMaxRecentFiles) {
		m_recentFiles.resize(static_cast<size_t>(kMaxRecentFiles));
	}
	persistRecentFiles();
}

void Mui::clearRecentFiles() {
	if (m_recentFiles.empty()) {
		return;
	}
	m_recentFiles.clear();
	persistRecentFiles();
}

void Mui::setRecentFileOpenHandler(std::function<void(const std::string &path)> handler) {
	m_recentOpenHandler = std::move(handler);
}

void Mui::registerExportAction(const std::string &label, std::function<void()> action) {
	m_exportActions.emplace_back(label, std::move(action));
}

void Mui::registerToolAction(const std::string &id, const std::string &label,
							 const std::string &shortcut, std::function<bool()> isActive,
							 std::function<void()> action) {
	for (auto &row : m_toolActions) {
		if (row.id == id) {
			row.label = label;
			row.shortcut = shortcut;
			row.isActive = std::move(isActive);
			row.action = std::move(action);
			return;
		}
	}
	m_toolActions.push_back(
		ToolMenuAction{id, label, shortcut, std::move(isActive), std::move(action)});
}

void Mui::registerPreferencesDrawer(const std::string &id, const std::string &label,
									std::function<void()> draw) {
	unregisterPreferencesDrawer(id);
	m_preferencesDrawers.push_back({id, label, std::move(draw)});
}

void Mui::unregisterPreferencesDrawer(const std::string &id) {
	m_preferencesDrawers.erase(
		std::remove_if(m_preferencesDrawers.begin(), m_preferencesDrawers.end(),
					   [&](const PreferencesDrawer &d) { return d.id == id; }),
		m_preferencesDrawers.end());
}

void Mui::requestExportPng() {
	m_exportPngPending = true;
}

void Mui::flushExportPng() {
	if (!m_exportPngPending || !m_engine) {
		return;
	}
	m_exportPngPending = false;
	const std::string path =
		exportFramebufferPng(m_engine->getFramebufferWidth(), m_engine->getFramebufferHeight());
	if (path.empty()) {
		showNotification("Export PNG failed", NotificationType::Error);
	} else {
		showNotification("Exported " + path, NotificationType::Success);
	}
}

void Mui::renderMainViewOverlays() {
	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImVec2 origin = viewport->WorkPos;
	ImVec2 size = viewport->WorkSize;
	if (m_statusBarHeight > 0.f && size.y > m_statusBarHeight) {
		size.y -= m_statusBarHeight;
	}

	if (m_rulersVisible) {
		const RulerUnit unit = clampRulerUnit(m_uiPrefs.rulerUnit);
		const float ppu = rulerPixPerDisplayUnit(unit, 1.f, m_dpiScale);
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		const float thick = rulerStripThickness(m_dpiScale);
		drawRulersInRegion(ImGui::GetForegroundDrawList(), origin, size, mouse, ppu,
						   rulerUnitLabel(unit), m_dpiScale, origin);

		// Host for strip hits + unit popup (DockSpace already ended). Pass through
		// clicks outside the bands so docked panels stay interactive.
		const bool onStrip = rulerHitStrip(origin, size, mouse, m_dpiScale);
		ImGui::SetNextWindowPos(origin);
		ImGui::SetNextWindowSize(size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGuiWindowFlags hitFlags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoFocusOnAppearing;
		if (!onStrip) {
			hitFlags |= ImGuiWindowFlags_NoInputs;
		}
		ImGui::Begin("##RulerHitHost", nullptr, hitFlags);
		ImGui::SetCursorScreenPos(origin);
		ImGui::InvisibleButton("##ruler_top", uiHitSize(ImVec2(size.x, thick)));
		const bool topClick = ImGui::IsItemClicked(ImGuiMouseButton_Right);
		ImGui::SetCursorScreenPos(origin);
		ImGui::InvisibleButton("##ruler_left", uiHitSize(ImVec2(thick, size.y)));
		const bool leftClick = ImGui::IsItemClicked(ImGuiMouseButton_Right);
		if (topClick || leftClick) {
			ImGui::OpenPopup("##RulerUnits");
		}
		if (rulerUnitPopup("##RulerUnits", m_uiPrefs.rulerUnit)) {
			m_uiPrefs.rulerUnit = static_cast<int>(clampRulerUnit(m_uiPrefs.rulerUnit));
			if (m_engine) {
				if (auto* settings = m_engine->getSettingsManager()) {
					settings->markDirty();
				}
			}
		}
		ImGui::End();
	}

	if (m_handles2D && m_engine && m_engine->getECSManager()) {
		// Identity content mapping for passthrough main view (1 content unit = 1 px).
		drawSelectedHandle2D(*m_engine->getECSManager(), origin.x, origin.y, 1.f);
	}
}

void Mui::installDefaultShortcuts() {
	if (m_defaultShortcutsReady) {
		return;
	}
	m_defaultShortcutsReady = true;

	m_shortcuts.setOnChanged([this] {
		if (!m_engine) {
			return;
		}
		if (auto* settings = m_engine->getSettingsManager()) {
			settings->setValue("shortcuts", m_shortcuts.exportOverrides());
		}
	});

	m_shortcuts.bind({"edit.undo", "Undo", ImGuiKey_Z, true, false, false, [this] {
						  if (m_undoStack && m_undoStack->canUndo()) {
							  m_undoStack->undo();
						  }
					  }});
	m_shortcuts.bind({"edit.redo", "Redo", ImGuiKey_Y, true, false, false, [this] {
						  if (m_undoStack && m_undoStack->canRedo()) {
							  m_undoStack->redo();
						  }
					  }});
	// Opt-in is a setup()-time call, so it is already settled by the first frame.
	if (editModeEnabled()) {
		m_shortcuts.bind({"view.edit_mode", kEditModeStatus, ImGuiKey_E, true, false, false,
						  [this] { setEditMode(!m_editMode); }});
	}
	m_shortcuts.bind({"view.rulers", "Rulers", ImGuiKey_F2, false, false, false,
					  [this] { m_rulersVisible = !m_rulersVisible; }});
	m_shortcuts.bind({"tools.select", "Select", ImGuiKey_V, false, false, false,
					  [this] { m_gizmoOp = GizmoOp::Select; }});
	m_shortcuts.bind({"tools.move", "Move", ImGuiKey_W, false, false, false,
					  [this] { m_gizmoOp = GizmoOp::Translate; }});
	m_shortcuts.bind({"tools.rotate", "Rotate", ImGuiKey_E, false, false, false,
					  [this] { m_gizmoOp = GizmoOp::Rotate; }});
	m_shortcuts.bind({"tools.scale", "Scale", ImGuiKey_R, false, false, false,
					  [this] { m_gizmoOp = GizmoOp::Scale; }});

	if (m_engine) {
		if (auto* settings = m_engine->getSettingsManager()) {
			m_shortcuts.importOverrides(settings->getValue("shortcuts"));
		}
	}
}

void Mui::ensureDefaultStatusSlots() {
	if (m_defaultStatusReady) {
		return;
	}
	m_defaultStatusReady = true;
	syncFpsChromeFromPrefs();
}

std::string Mui::fpsStatusText() const {
	if (!m_engine) {
		return "FPS --";
	}
	const float dt = m_engine->getDeltaTime();
	if (dt <= 0.f) {
		return "FPS --";
	}
	char buf[32];
	std::snprintf(buf, sizeof(buf), "FPS %.0f", 1.f / dt);
	return buf;
}

void Mui::syncFpsChromeFromPrefs() {
	const bool fpsOnStatusBar = statusBarVisible() && fpsDisplayMode() == 0;
	if (fpsOnStatusBar) {
		m_statusBar.setSlot({"fps", [this]() -> std::string { return fpsStatusText(); }, 80.f});
	} else {
		m_statusBar.removeSlot("fps");
	}
}

void Mui::drawProgressInStatusBar(bool sameLine) {
	if (!m_progress.useStatusBar()) {
		return;
	}
	const Progress::Snapshot snap = m_progress.snapshot();
	if (!snap.visible) {
		return;
	}

	if (sameLine) {
		ImGui::SameLine(0, 16.f);
	}
	ImGui::TextUnformatted(snap.title.c_str());
	ImGui::SameLine(0, 10.f);

	const float barW = std::max(160.f, ImGui::GetContentRegionAvail().x * 0.42f);
	ImGui::ProgressBar(snap.progress, ImVec2(barW, 0.f));

	ImGui::SameLine(0, 10.f);
	ImGui::TextUnformatted(snap.label.c_str());

	if (snap.cancelable && snap.active) {
		ImGui::SameLine(0, 10.f);
		if (ImGui::SmallButton("Cancel##rigProgressStatus")) {
			m_progress.requestCancel();
		}
	}
}

void Mui::renderProgressFloating() {
	if (m_progress.useStatusBar()) {
		return;
	}
	const Progress::Snapshot snap = m_progress.snapshot();
	if (!snap.visible) {
		return;
	}

	ImGuiViewport *vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		ImVec2(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f),
		ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(420.f, 0.f), ImGuiCond_Appearing);

	const ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_AlwaysAutoResize;
	bool open = true;
	if (!ImGui::Begin("Progress###rigProgressFloat", &open, flags)) {
		ImGui::End();
		return;
	}
	if (!open) {
		m_progress.hide();
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted(snap.title.c_str());
	ImGui::ProgressBar(snap.progress, ImVec2(-1.f, 0.f));
	ImGui::TextUnformatted(snap.label.c_str());

	if (snap.cancelable && snap.active) {
		if (ImGui::Button("Cancel##rigProgressFloat", ImVec2(-1.f, 0.f))) {
			m_progress.requestCancel();
		}
	}
	ImGui::End();
}

void Mui::renderStatusBar() {
	if (!statusBarVisible()) {
		return;
	}
	const ImGuiViewport *viewport = ImGui::GetMainViewport();
	const float h =
		(m_statusBarHeight > 0.f) ? m_statusBarHeight : ImGui::GetFrameHeight();
	ImVec2 pos = viewport->WorkPos;
	pos.y += viewport->WorkSize.y - h;
	ImGui::SetNextWindowPos(pos);
	ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, h));
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 2.f));
	if (ImGui::Begin("##rigImGuiStatusBar", nullptr, flags)) {
		bool leftDrawn = false;
		if (m_editMode) {
			ImGui::TextUnformatted(kEditModeStatus);
			leftDrawn = true;
		}
		if (!m_statusBar.left().empty()) {
			if (leftDrawn) {
				ImGui::SameLine(0, 12.f);
			}
			ImGui::TextUnformatted(m_statusBar.left().c_str());
			leftDrawn = true;
		}
		drawProgressInStatusBar(leftDrawn);

		float right = 0.f;
		for (const auto &slot : m_statusBar.slots()) {
			right += (slot.width > 0.f) ? slot.width : 72.f;
		}
		if (right > 0.f) {
			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - right - 8.f);
		}
		for (size_t i = 0; i < m_statusBar.slots().size(); ++i) {
			const auto &slot = m_statusBar.slots()[i];
			const std::string text = slot.text ? slot.text() : std::string{};
			ImGui::TextUnformatted(text.c_str());
			if (i + 1 < m_statusBar.slots().size()) {
				ImGui::SameLine();
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar(3);
}

} // namespace rigkit

