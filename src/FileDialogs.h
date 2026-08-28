#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>
#include <imfilebrowser.h>

#include "core/util/AppPaths.h"

#include "UiDpi.h"

namespace rigkit {

/** Design (1x) size of the open/save browser - pass through uiPx at open.
 * Room for the quick-access sidebar plus a usable file list. */
inline constexpr int kFileDialogDesignW = 1100;
inline constexpr int kFileDialogDesignH = 700;
/** Quick-access sidebar width in 1x design units. */
inline constexpr float kFileDialogQuickAccessDesignW = 160.f;

/**
 * @brief DPI-scaled size for any ImGui::FileBrowser (Place / Open / Save / ...).
 * @details Centering is handled in imfilebrowser when no explicit SetWindowPos
 * was set. Call before Open() so Appearing size picks up the new layout.
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
inline void setFileBrowserFilters(ImGui::FileBrowser& browser,
								  std::vector<std::string> filters) {
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
 * @brief Open/save browsers for Mui - keeps ImGui::FileBrowser out of IMui.
 * @details Opening size uses uiPx (design × FontScaleDpi), clamped to the
 * work area. Undersized imgui.ini crumbs are bumped up on open.
 */
class FileDialogs {
  public:
	using Callback = std::function<void(const std::string& path)>;

	FileDialogs();

	void open(const std::string& title, std::vector<std::string> filters, Callback onSelected);
	void save(const std::string& title, std::vector<std::string> filters, Callback onSelected);

	/** @brief Draw browsers and fire callback when a path is chosen. */
	void tick();

  private:
	void applyLayout();
	/** @brief Append the active type-filter extension when the name lacks it.
	 * @details Concrete combo only (for example `.svg`); all-files skips.
	 * No double suffix. */
	std::string withSaveExtension(std::filesystem::path path) const;

	enum class Mode { None, Open, Save };

	ImGui::FileBrowser m_open;
	ImGui::FileBrowser m_save;
	Mode m_mode = Mode::None;
	Callback m_callback;
	int m_skipDisplay = 0;	 ///< File menu is still a popup this frame - OpenPopup next tick.
	bool m_openedOnce = false; ///< True after BeginPopupModal succeeded; then !open means cancel.
};

} // namespace rigkit
