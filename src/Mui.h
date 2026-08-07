#pragma once

#include <deque>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector> // UiPrefs::GetProperties
#include "FileDialogs.h"
#include "HostMenuBar.h"
#include "ImGuiStyleKit.h"
#include "MWindow.h"
#include "Rulers.h"
#include "ShortcutManager.h"
#include "StatusBar.h"
#include "core/IMui.h"
#include "core/json.h"
#include "core/util/Progress.h"
#include "ecs/PropertyReflection.h"

struct GLFWwindow;
struct ImFontAtlas;

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

	/**
	 * @brief Save the current dock layout as a named workspace.
	 * @details Workspaces are layout snapshots stored as `<name>.ini` under
	 * AppPaths::getWorkspacesDir(). The live session keeps autosaving to
	 * `imgui.ini`; switching workspaces replaces the session layout. The active
	 * name persists in user settings across runs.
	 * @return False when the name is invalid or the file could not be written.
	 */
	bool saveWorkspace(const std::string& name);
	/**
	 * @brief Apply a saved workspace at the next frame boundary.
	 * @return False when the name is invalid or no such workspace exists.
	 */
	bool loadWorkspace(const std::string& name);
	/// Remove a saved workspace. Clears the active name when it matches.
	bool deleteWorkspace(const std::string& name);
	/// Active workspace name; empty when the session layout is unnamed.
	const std::string& currentWorkspace() const { return m_currentWorkspace; }
	/// Saved workspace names, sorted (session `imgui.ini` excluded).
	std::vector<std::string> workspaceNames() const;

	/**
	 * @brief Create (if needed) and show one built-in host panel.
	 * @details Idempotent. Does not install a dock layout — call
	 * setFirstRunHostDockLayout / setDockLayoutBuilder separately when needed.
	 */
	void addHostPanel(HostPanel panel);
	/** @brief Create and show several host panels. */
	void addHostPanels(std::initializer_list<HostPanel> panels);
	/**
	 * @brief Create every host panel at sketch defaults and install first-run
	 * dock layout when no builder is set.
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
	 * @brief First-run host dock layout; no-ops when imgui.ini already restored a split.
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

	Progress* progress() override { return &m_progress; }

	/**
	 * @brief Extra Preferences sections (ImGui draw; not MSettings persistence).
	 * @details Use for project-scoped UI (e.g. plot Document/Canvas) that should
	 * appear under File → Preferences without writing rigkit_settings.json.
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
	 * @brief Screen rect of the empty central dock (GL bed) when passthrough is on.
	 * @return false if unavailable (falls back to full work area).
	 */
	bool centralViewRect(float& outX, float& outY, float& outW, float& outH) const;

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

	/** @brief Host FPS text for status bar / menu bar chrome. */
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
	UiPrefs m_uiPrefs;
	std::string m_appliedFontFile;
	float m_appliedFontSize = -1.f;
	std::vector<std::function<void(ImFontAtlas&)>> m_fontAtlasHooks;
};

} // namespace rigkit
