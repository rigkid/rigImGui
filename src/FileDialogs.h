#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>
#include <imfilebrowser.h>

#include "core/IMui.h"
#include "core/util/AppPaths.h"

#include "IWindow.h"
#include "ThumbnailCache.h"
#include "UiDpi.h"

struct GLFWwindow;

namespace rigkit {

/** Design (1x) size of the open/save browser - pass through uiPx at open.
 * Room for the quick-access sidebar plus a usable file list. */
inline constexpr int kFileDialogDesignW = 1100;
inline constexpr int kFileDialogDesignH = 700;
/** Quick-access sidebar width in 1x design units. */
inline constexpr float kFileDialogQuickAccessDesignW = 160.f;

/**
 * @brief DPI-scaled size for any ImGui::FileBrowser (Place / Open / Save / ...).
 * @details Sidebar width lives here. The IWindow host sets position and size
 * on Begin. Call before Open() so Appearing size picks up the new layout.
 */
inline void applyFileBrowserLayout(ImGui::FileBrowser& browser) {
	int w = 0;
	int h = 0;
	uiWindowSize(kFileDialogDesignW, kFileDialogDesignH, w, h);
	browser.SetWindowSize(w, h);
	browser.SetQuickAccessWidth(uiPx(kFileDialogQuickAccessDesignW));
}

/** @brief App data / User data / Home / Desktop / Documents / Downloads shortcuts (left sidebar).
 * @details Call once per browser. AddQuickAccess stats each folder; Desktop /
 * Documents / Downloads on OneDrive can stall the UI if this runs every Open. */
inline void installFileBrowserQuickAccess(ImGui::FileBrowser& browser) {
	namespace fs = std::filesystem;
	browser.ClearQuickAccess();

	auto addIfDir = [&](const char* label, const fs::path& p) {
		if (!p.empty()) {
			browser.AddQuickAccess(label, p);
		}
	};

	// The running app's data folder first - it is where app documents live.
	const std::string data = AppPaths::getDataDir();
	if (!data.empty()) {
		addIfDir("App data", fs::path(data));
	}
	const std::string userData = AppPaths::getUserDataDir();
	if (!userData.empty()) {
		addIfDir("User data", fs::path(userData));
	}

	const char* homeEnv =
#if defined(_WIN32)
		std::getenv("USERPROFILE");
#else
		std::getenv("HOME");
#endif
	const fs::path home = homeEnv && *homeEnv ? fs::path(homeEnv) : fs::path{};
	if (!home.empty()) {
		addIfDir("Home", home);
		addIfDir("Desktop", home / "Desktop");
		addIfDir("Documents", home / "Documents");
		addIfDir("Downloads", home / "Downloads");
	}
}

/**
 * @brief Apply extension filters and always offer an all-files choice in the combo.
 * @details imgui-filebrowser's any-extension token is not the Windows all-files
 * glob. Callers pass preferred types first; all-files is appended when missing
 * so the directory listing can still show everything.
 */
inline void setFileBrowserFilters(ImGui::FileBrowser& browser, std::vector<std::string> filters) {
	for (auto& f : filters) {
		if (f == "*.*") {
			f = ".*";
		}
	}
	const bool hasAll =
		std::any_of(filters.begin(), filters.end(), [](const std::string& f) { return f == ".*"; });
	if (!hasAll) {
		filters.push_back(".*");
	}
	browser.SetTypeFilters(filters);
}

/**
 * @brief Viewport-capable chrome for FileDialogs.
 * @details IWindow::Begin is a regular window, so multi-viewport can tear it
 * into its own OS window. Listing stays in FileBrowser::DrawContents().
 */
class FileDialogWindow : public IWindow {
  public:
	FileDialogWindow();
	void setBrowser(ImGui::FileBrowser* browser);
	/** @brief Path dropped on the browser this frame. Empty skips. */
	void offerDroppedPath(std::filesystem::path path);
	void render() override;
	/** @brief True after this frame's Begin succeeded and listing was drawn. */
	bool drewContents() const;

  protected:
	void renderContents() override;

  private:
	ImGui::FileBrowser* m_browser = nullptr;
	bool m_drewContents = false;
	std::filesystem::path m_droppedPath;
};

/**
 * @brief Open/save browsers for Mui - keeps ImGui::FileBrowser out of IMui.
 * @details Opening size uses uiPx (design × FontScaleDpi), clamped to the
 * work area. Undersized imgui.ini crumbs are bumped up on open. Hosted as
 * FileDialogWindow, not a FileBrowser popup.
 */
class FileDialogs {
  public:
	using Callback = std::function<void(const std::string& path)>;
	using MultiCallback = std::function<void(const std::vector<std::string>& paths)>;

	FileDialogs();
	~FileDialogs();

	/** @brief Drop GPU thumbs while the GL context is still current. */
	void shutdown();

	void open(const std::string& title, std::vector<std::string> filters, Callback onSelected);
	/**
	 * @brief Open dialog that can return more than one file.
	 * @details Ctrl+click, Shift+click, and Ctrl+A. One file still confirms.
	 * Does not apply to save or folder dialogs. Cancel runs no callback.
	 */
	void openMany(const std::string& title, std::vector<std::string> filters,
				  MultiCallback onSelected);
	/** @param defaultName Pre-filled file name (Save As suggests the current one). */
	void save(const std::string& title, std::vector<std::string> filters, Callback onSelected,
			  const std::string& defaultName = {});
	/** @param startDir Directory the browser opens in. Empty keeps the last one. */
	void openFolder(const std::string& title, Callback onSelected,
					const std::string& startDir = {});

	/** @brief Draw browsers and fire callback when a path is chosen. */
	void tick();

	/**
	 * @brief Route OS file drops to the open browser.
	 * @details Installs the GLFW drop callback on @p window and on ImGui
	 * viewport windows. Call after the GLFW window exists, and again after
	 * viewports are created. A drop whose cursor is over the browser jumps
	 * to that folder, or to a file's parent and selects the file. The dialog
	 * stays open. A multi-file drop uses the first path. Drops while no
	 * dialog is open go to setUnclaimedFileDropHandler.
	 */
	void installDropTarget(GLFWwindow* window = nullptr);

	/**
	 * @brief Receive OS file drops while no open/save/folder dialog is open.
	 * @details Paths are native narrow strings, the same form Open returns.
	 * An empty handler clears it. The open dialog still consumes a drop.
	 */
	using DropHandler = std::function<void(const std::vector<std::string>& paths)>;
	void setUnclaimedFileDropHandler(DropHandler handler);

	/** @brief Replace the user-pinned Quick access folders on both browsers.
	 * @param paths UTF-8 folder paths; missing folders are skipped. */
	void setPinnedFolders(const std::vector<std::string>& paths);
	/** @return Current user-pinned Quick access folders (UTF-8). */
	std::vector<std::string> pinnedFolders() const;
	/** @return True once after the user pinned or unpinned a folder in either
	 * browser (right-click a folder row / a pinned sidebar entry). The host
	 * persists pinnedFolders() when this fires. */
	bool consumePinnedFoldersChanged();

	void setViewMode(ImGui::FileBrowser::ViewMode mode);
	ImGui::FileBrowser::ViewMode viewMode() const;
	/** @return True once after the user toggled Details / Icons. The host
	 * persists viewMode() when this fires. */
	bool consumeViewModeChanged();

	/** @brief Pack-registered CPU decoder for one file extension. */
	void registerThumbnailDecoder(const std::string& extension, IMui::FileThumbnailDecoder decoder);

  private:
	void applyLayout();
	void showOpen(const std::string& title, std::vector<std::string> filters, bool many);
	/** @brief Append the active type-filter extension when the name lacks it.
	 * @details Concrete combo only (for example `.svg`); all-files skips.
	 * No double suffix. */
	std::string withSaveExtension(std::filesystem::path path) const;

	enum class Mode { None, Open, Save, Folder };

	ImGui::FileBrowser m_open;
	ImGui::FileBrowser m_save;
	ImGui::FileBrowser m_folder;
	FileDialogWindow m_window;
	ThumbnailCache m_thumbs;
	Mode m_mode = Mode::None;
	bool m_multi = false;
	Callback m_callback;
	MultiCallback m_multiCallback;
	DropHandler m_unclaimedDrop;
	bool m_pinsChanged = false; ///< User pinned/unpinned since last consume.
	bool m_viewChanged = false; ///< User toggled Details / Icons since last consume.
};

} // namespace rigkit
