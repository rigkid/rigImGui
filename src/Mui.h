#pragma once

#include <deque>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector> // UiPrefs::GetProperties
#include "FileDialogs.h"
#include "HostMenuBar.h"
#include "ImGuiStyleKit.h"
#include "MWindow.h"
#include "Rulers.h"
#include "ShortcutManager.h"
#include "StatusBar.h"
#include "TtfKern.h"
#include "core/IMui.h"
#include "core/json.h"
#include "core/util/Progress.h"
#include "ecs/PropertyReflection.h"

struct GLFWwindow;
struct ImFontAtlas;
struct ImGuiContext;
struct ImGuiSettingsHandler;
struct ImGuiTextBuffer;

namespace rigkit {
class RigKitEngine;
class UndoStack;
namespace ecs {
class SEvent;
}
} // namespace rigkit

namespace rigkit {

struct Notification {
	std::string message;
	NotificationType type;
	float timeRemaining;
};

struct Modal {
	std::string title;
	std::string message;
	NotificationType type;
	std::function<void()> onOk;
	bool open = true;
};

/** @brief Built-in host panels created on demand by Mui::addHostPanel. */
enum class HostPanel {
	Log,
	Windows,
	Debug,
	Properties,
	Scene,
	Layers,
	Viewport,
	Shortcuts,
	Theme,
	Preferences
};

/// Pack-owned preference POD (registered on MSettings as "rigImGui.ui" / label "Interface").
struct UiPrefs {
	int theme = 0; // ImGuiTheme as int (0=Dark … 4=Dracula)
	std::string themeFile; // empty = built-in; else path under data/user/themes or absolute
	std::string fontFile;  // empty = Roboto; else TTF under data/fonts or absolute
	float fontSize = 16.0f;
	bool chromeKerning = true; ///< Pair kerning on chrome labels (TTF `kern` or setChromeKernFn)
	bool confirmQuit = false;
	float notificationSeconds = 3.0f;
	float notificationWidth = 320.f; ///< logical px; clamped 160–600, scaled by DPI
	int rulerUnit = static_cast<int>(RulerUnit::Mm); // default mm for plotter hosts
	float rulerThickness = 18.f;
	bool rulerMinorTicks = true;
	float rulerMajorPx = 50.f; ///< target pixels between major ticks
	bool progressUseStatusBar = true;
	float progressAutoHideSeconds = 2.0f;
	bool showStatusBar = true;
	/// 0 = Status Bar, 1 = Menu Bar, 2 = Off
	int fpsDisplay = 0;

	std::vector<sProp> GetProperties() {
		static constexpr const char* kThemeNames[] = {
			"Dark", "Light", "Classic", "Corporate", "Dracula",
		};
		static constexpr const char* kFpsDisplayNames[] = {
			"Status Bar",
			"Menu Bar",
			"Off",
		};
		static_assert(sizeof(kThemeNames) / sizeof(kThemeNames[0]) == kImGuiThemeCount);
		static_assert(sizeof(kRulerUnitNames) / sizeof(kRulerUnitNames[0]) == kRulerUnitCount);
		return {
			{0, "Theme", EPT_ENUM, &theme, kThemeNames, kImGuiThemeCount},
			{1, "Theme File", EPT_STRING, &themeFile},
			{2, "Font File", EPT_STRING, &fontFile},
			{3, "Font Size", EPT_FLOAT, &fontSize},
			{15, "Chrome Kerning", EPT_BOOL, &chromeKerning},
			{4, "Confirm Quit", EPT_BOOL, &confirmQuit},
			{5, "Notification Seconds", EPT_FLOAT, &notificationSeconds},
			{14, "Notification Width", EPT_FLOAT, &notificationWidth},
			{6, "Ruler Unit", EPT_ENUM, &rulerUnit, kRulerUnitNames, kRulerUnitCount},
			{7, "Ruler Thickness", EPT_FLOAT, &rulerThickness},
			{8, "Ruler Minor Ticks", EPT_BOOL, &rulerMinorTicks},
			{9, "Ruler Major Spacing (px)", EPT_FLOAT, &rulerMajorPx},
			{10, "Progress In Status Bar", EPT_BOOL, &progressUseStatusBar},
			{11, "Progress Auto-Hide Seconds", EPT_FLOAT, &progressAutoHideSeconds},
			{12, "Show Status Bar", EPT_BOOL, &showStatusBar},
			{13, "FPS Display", EPT_ENUM, &fpsDisplay, kFpsDisplayNames, 3},
		};
	}
};

/// rigImGui IMui fulfillment — default RigKit UI pack.
/// Themes/fonts/style extras: ImGuiStyleKit.
class Mui : public IMui {
  public:
	Mui();
	~Mui() override;

	void init() override;
	void shutdown() override;
	void handleInput() override;
	void render() override;
	void setRigKitEngine(RigKitEngine *engine) override;

	RigKitEngine *getRigKitEngine() const override { return m_engine; }

	void setWindowVisibility(const std::string &windowName,
							 bool visible) override;
	void setWindowVisibilityAll(bool visible) override;
	bool getWindowVisibility(const std::string &windowName) const override;
	std::vector<std::string> getAllWindowNames() const override;

	MWindow *getWindowManager() override;
	const MWindow *getWindowManager() const override;

	void initImGui();
	void shutdownImGui();
	void setupDockspace();
	void renderAllWindows();

	void setImGuiTheme(ImGuiTheme theme);
	ImGuiTheme getImGuiTheme() const { return m_currentTheme; }
	/// Save / load custom theme JSON. Relative paths resolve under AppPaths::getThemesDir().
	bool saveCurrentTheme(const std::string& path, bool notify = true);
	bool loadTheme(const std::string& path, bool notify = true);
	/// Apply UiPrefs theme enum + optional themeFile / font settings.
	/// Font atlas rebuild is deferred until the next frame (safe mid-UI).
	void applyUiPrefs();
	/// Reload body font (+ icons) from UiPrefs. Defers if a frame is open.
	bool reloadFonts();
	/// Bind pair kerning on the body ImFont (TTF `kern` or setChromeKernFn).
	void bindChromeKerning();
	void loadChromeKernTable();

	/**
	 * @brief Save the current dock layout and window visibility as a named workspace.
	 * @details Workspaces are snapshots stored as `<name>.ini` under
	 * AppPaths::getWorkspacesDir() (ImGui dock tree plus `[RigVisibility]`).
	 * The live session keeps autosaving to `imgui.ini`. Active name persists in
	 * user settings; startup reloads that named workspace (or `Standard`).
	 * @param notify When true, show a success/error notification.
	 * @return False when the name is invalid or the file could not be written.
	 */
	bool saveWorkspace(const std::string& name, bool notify = true);
	/**
	 * @brief Apply a saved workspace at the next frame boundary.
	 * @details Restores dock layout and, when the file has `[RigVisibility]`,
	 * hides every registered window then shows only those marked visible.
	 * @param notify When true, warn if the workspace file is missing.
	 * @return False when the name is invalid or no such workspace exists.
	 */
	bool loadWorkspace(const std::string& name, bool notify = true);
	/// Remove a saved workspace. `Standard` and session `imgui` cannot be deleted.
	bool deleteWorkspace(const std::string& name);
	/// Active workspace name; empty when the session layout is unnamed.
	const std::string& currentWorkspace() const { return m_currentWorkspace; }
	/// Saved workspace names, sorted (session `imgui.ini` excluded).
	std::vector<std::string> workspaceNames() const;
	/// Built-in default workspace name (seeded once from the first settled layout).
	static constexpr const char* kStandardWorkspace = "Standard";

	/**
	 * @brief Create (if needed) and show one built-in host panel.
	 * @details Idempotent. Does not install a dock layout — call
	 * setFirstRunHostDockLayout / setDockLayoutBuilder separately when needed.
	 */
	void addHostPanel(HostPanel panel);
	/** @brief Create and show several host panels. */
	void addHostPanels(std::initializer_list<HostPanel> panels);
	/**
	 * @brief Create every host panel at sketch defaults.
	 * @details Installs first-run dock layout only when no builder is set and
	 * `Standard.ini` is missing (seed path). Prefer loading `Standard` / last
	 * used workspace on startup once that file exists.
	 */
	void addAllHostPanels();

	/// Ensure Preferences exists and show it (File → Preferences…).
	void showPreferences();

	/**
	 * @brief Show a transient notification in the host chrome.
	 * @param message Body text (wrapped to Notification Width).
	 * @param type Severity tint.
	 * @param duration Seconds visible; negative uses UiPrefs::notificationSeconds.
	 */
	void showNotification(const std::string &message,
						  NotificationType type = NotificationType::Info,
						  float duration = -1.f);
	void showModal(const std::string &title, const std::string &message,
				   NotificationType type = NotificationType::Info,
				   std::function<void()> onOk = nullptr);

	/** @brief Help → About — RigKit blurb + loaded packs (description, license). */
	void showAbout();

	void setDockPassthroughCentral(bool enabled) override {
		m_dockPassthroughCentral = enabled;
	}
	bool dockPassthroughCentral() const override { return m_dockPassthroughCentral; }

	/**
	 * @brief Build default docks once before DockSpace submits (same GetID scope).
	 * @details Called each frame until the builder clears itself / no-ops. Packs
	 * that need an OF-style first layout register here from setup().
	 */
	void setDockLayoutBuilder(std::function<void(ImGuiID dockspaceId)> fn) {
		m_dockLayoutBuilder = std::move(fn);
	}

	/**
	 * @brief One-shot host dock layout used to seed `Standard.ini` when missing.
	 * @details No-ops when a dock split already exists (restored ini / prior seed).
	 * @param extraRightWindows Optional titles docked with Properties on the right
	 * (e.g. app "Show Control"). Call after those windows exist.
	 */
	void setFirstRunHostDockLayout(std::vector<std::string> extraRightWindows = {});

	void setUndoStack(UndoStack *stack) override { m_undoStack = stack; }
	UndoStack *undoStack() const override { return m_undoStack; }

	void setEditMode(bool enabled) override;
	bool editMode() const override { return m_editMode; }

	/// Reads the engine's opt-in flag — Mui keeps no copy.
	bool editModeEnabled() const;

	void openFileDialog(const std::string &title, std::vector<std::string> filters,
						FileDialogCallback onSelected) override;
	void saveFileDialog(const std::string &title, std::vector<std::string> filters,
						FileDialogCallback onSelected) override;

	void registerFileAction(const std::string &label, std::function<void()> action,
							const std::string &shortcut = {}) override;
	void registerFileSubmenu(const std::string &label,
							 std::function<void()> drawContents) override;
	void registerAppAction(const std::string &label, std::function<void()> action,
						   const std::string &shortcut = {}) override;
	void registerAppSubmenu(const std::string &label,
							std::function<void()> drawContents) override;
	void noteRecentFile(const std::string &path) override;
	void clearRecentFiles() override;
	void setRecentFileOpenHandler(
		std::function<void(const std::string &path)> handler) override;
	void registerExportAction(const std::string &label,
							  std::function<void()> action) override;
	void registerToolAction(const std::string &id, const std::string &label,
							const std::string &shortcut, std::function<bool()> isActive,
							std::function<void()> action) override;
	void setGizmoOp(GizmoOp op) override { m_gizmoOp = op; }
	GizmoOp gizmoOp() const override { return m_gizmoOp; }

	void registerFontAtlasHook(std::function<void(ImFontAtlas& atlas)> hook) override;

	void setChromeKerning(bool enabled) override;
	bool chromeKerning() const override { return m_uiPrefs.chromeKerning; }
	void setChromeKernFn(ChromeKernFn fn, void* user) override;
	int chromeKernPairCount() const override { return m_ttfKern.pairCount(); }

	Progress* progress() override { return &m_progress; }

	/**
	 * @brief Extra Preferences sections (ImGui draw; not MSettings persistence).
	 * @details Use for project-scoped UI (e.g. plot Document/Canvas) that should
	 * appear under App → Preferences without writing rigkit_settings.json.
	 */
	void registerPreferencesDrawer(const std::string &id, const std::string &label,
								   std::function<void()> draw);
	void unregisterPreferencesDrawer(const std::string &id);
	struct PreferencesDrawer {
		std::string id;
		std::string label;
		std::function<void()> draw;
	};
	const std::vector<PreferencesDrawer> &preferencesDrawers() const {
		return m_preferencesDrawers;
	}

	/** @brief Optional per-frame gizmo draw (apps wire MeshGizmo here). */
	void setGizmoDrawer(std::function<void(float x, float y, float w, float h, GizmoOp op)> drawer) {
		m_gizmoDrawer = std::move(drawer);
	}

	/**
	 * @brief Window-client rect of the empty central dock (GL bed) when valid.
	 * @details Subtracts main viewport origin so coords match GLFW cursor space.
	 */
	bool centralViewRect(float& outX, float& outY, float& outW, float& outH) const override;

	ShortcutManager &shortcuts() { return m_shortcuts; }
	const ShortcutManager &shortcuts() const { return m_shortcuts; }
	StatusBar &statusBar() { return m_statusBar; }
	const StatusBar &statusBar() const { return m_statusBar; }

	/**
	 * @brief Optional right-aligned status text in the host main menu bar.
	 * @details Called each frame while the menu bar is open. Empty string hides it.
	 */
	void setMenuBarRightStatus(std::function<std::string()> text) {
		m_menuBarRightStatus = std::move(text);
	}
	const std::function<std::string()> &menuBarRightStatus() const {
		return m_menuBarRightStatus;
	}

	bool rulersVisible() const { return m_rulersVisible; }
	void setRulersVisible(bool v) { m_rulersVisible = v; }
	bool handles2DVisible() const { return m_handles2D; }
	void setHandles2DVisible(bool v) { m_handles2D = v; }

	/** @brief Queue a full-window PNG export after this frame's UI present. */
	void requestExportPng();

	struct FileMenuAction {
		std::string label;
		std::string shortcut;
		std::function<void()> action;
		bool submenu = false; ///< When true, @c action draws nested menu contents.
	};

	struct ToolMenuAction {
		std::string id;
		std::string label;
		std::string shortcut;
		std::function<bool()> isActive;
		std::function<void()> action;
	};

	const std::vector<FileMenuAction> &fileActions() const { return m_fileActions; }
	const std::vector<FileMenuAction> &appActions() const { return m_appActions; }
	const std::vector<ToolMenuAction> &toolActions() const { return m_toolActions; }
	const std::vector<std::string> &recentFiles() const { return m_recentFiles; }
	const std::function<void(const std::string &)> &recentFileOpenHandler() const {
		return m_recentOpenHandler;
	}

	const std::vector<std::pair<std::string, std::function<void()>>> &exportActions() const {
		return m_exportActions;
	}

	UiPrefs &uiPrefs() { return m_uiPrefs; }
	const UiPrefs &uiPrefs() const { return m_uiPrefs; }
	float dpiScale() const { return m_dpiScale; }

	/** @brief Host FPS text for status bar / menu bar chrome (smoothed). */
	std::string fpsStatusText() const;

  private:
	void renderNotifications();
	void renderModals();
	void renderAbout();
	void renderStatusBar();
	void renderProgressFloating();
	void drawProgressInStatusBar(bool sameLine);
	void renderMainViewOverlays();
	void tickNotifications(float dt);
	void applyDpiStyle();
	void installDefaultShortcuts();
	void ensureDefaultStatusSlots();
	void syncFpsChromeFromPrefs();
	void flushExportPng();
	void syncProgressFromPrefs();
	void loadRecentFilesFromSettings();
	void persistRecentFiles();
	void persistCurrentWorkspace();
	void registerVisibilitySettingsHandler();
	void queueStartupWorkspace();
	void applyPendingWindowVisibility();
	void maybeSeedStandardWorkspace();
	static bool workspaceFileExists(const std::string& name);

	static void RigVisibility_ClearAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler);
	static void RigVisibility_ReadInit(ImGuiContext* ctx, ImGuiSettingsHandler* handler);
	static void* RigVisibility_ReadOpen(ImGuiContext* ctx, ImGuiSettingsHandler* handler,
										const char* name);
	static void RigVisibility_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler,
									   void* entry, const char* line);
	static void RigVisibility_ApplyAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler);
	static void RigVisibility_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler,
									   ImGuiTextBuffer* out_buf);

	static const char* hostPanelTitle(HostPanel panel);
	std::shared_ptr<IWindow> ensureHostPanel(HostPanel panel);
	bool statusBarVisible() const { return m_uiPrefs.showStatusBar; }
	/// 0 Status Bar, 1 Menu Bar, 2 Off
	int fpsDisplayMode() const {
		return (m_uiPrefs.fpsDisplay < 0 || m_uiPrefs.fpsDisplay > 2) ? 0 : m_uiPrefs.fpsDisplay;
	}

	RigKitEngine *m_engine = nullptr;
	std::unique_ptr<MWindow> m_windowManager;
	std::unique_ptr<HostMenuBar> m_menuBar;
	GLFWwindow *m_window = nullptr;
	ImGuiTheme m_currentTheme = ImGuiTheme::Dark;
	std::string m_currentWorkspace;	   ///< Active workspace name; empty = unnamed
	std::string m_pendingWorkspaceLoad; ///< Ini path applied before next NewFrame
	std::string m_iniPath; ///< Stable storage for io.IniFilename pointer
	std::unordered_map<std::string, bool> m_pendingVisibility; ///< From [RigVisibility]
	bool m_rigVisibilitySectionSeen = false; ///< Set while parsing a RigVisibility block
	bool m_applyPendingVisibility = false;	 ///< Apply hide-all / show-listed after LoadIni
	bool m_startupWorkspaceQueued = false;	 ///< First-frame named workspace load
	bool m_standardSeedAttempted = false;	 ///< One-shot Standard.ini creation

	std::deque<Notification> m_notifications;
	std::deque<Modal> m_modals;
	bool m_aboutOpen = false;

	UndoStack *m_undoStack = nullptr;
	ShortcutManager m_shortcuts;
	StatusBar m_statusBar;
	Progress m_progress;
	std::function<std::string()> m_menuBarRightStatus;
	FileDialogs m_fileDialogs;
	bool m_editMode = false; ///< IN Edit Mode = author panels rendered
	bool m_defaultShortcutsReady = false;
	bool m_defaultStatusReady = false;
	bool m_rulersVisible = false;
	bool m_handles2D = true;
	bool m_exportPngPending = false;
	GizmoOp m_gizmoOp = GizmoOp::Select;
	std::function<void(float, float, float, float, GizmoOp)> m_gizmoDrawer;
	std::vector<FileMenuAction> m_fileActions;
	std::vector<FileMenuAction> m_appActions;
	std::vector<ToolMenuAction> m_toolActions;
	std::vector<std::pair<std::string, std::function<void()>>> m_exportActions;
	std::vector<PreferencesDrawer> m_preferencesDrawers;
	std::vector<std::string> m_recentFiles;
	std::function<void(const std::string &)> m_recentOpenHandler;
	static constexpr int kMaxRecentFiles = 10;

	bool m_imguiReady = false;
	bool m_dockPassthroughCentral = false;
	bool m_inImGuiFrame = false;
	bool m_pendingFontReload = false;
	std::function<void(ImGuiID)> m_dockLayoutBuilder;
	ImGuiID m_dockspaceId = 0;
	float m_centralX = 0.f;
	float m_centralY = 0.f;
	float m_centralW = 0.f;
	float m_centralH = 0.f;
	bool m_centralValid = false;
	float m_dpiScale = 1.f;
	float m_statusBarHeight = 0.f;
	float m_fpsShown = 0.f;	 ///< Averaged FPS for chrome text
	float m_fpsAccum = 0.f;	 ///< Seconds in the current FPS sample window
	int m_fpsFrames = 0;	 ///< Frames in the current FPS sample window
	UiPrefs m_uiPrefs;
	std::string m_appliedFontFile;
	float m_appliedFontSize = -1.f;
	TtfKern m_ttfKern;
	ChromeKernFn m_chromeKernFn = nullptr;
	void* m_chromeKernUser = nullptr;
	std::vector<std::function<void(ImFontAtlas&)>> m_fontAtlasHooks;
};

} // namespace rigkit
