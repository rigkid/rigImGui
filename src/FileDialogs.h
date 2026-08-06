#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <imgui.h>
#include <imfilebrowser.h>
#include <string>
#include <vector>

#include "core/util/AppPaths.h"

namespace rigkit {

/** ImGui/Display size of the open/save browser (same units as GLFW window size). */
inline constexpr int kFileDialogDesignW = 640;
inline constexpr int kFileDialogDesignH = 400;
/** Quick-access sidebar width in the same units. */
inline constexpr float kFileDialogQuickAccessDesignW = 140.f;

/** @brief App data / Home / Desktop / Documents / Downloads shortcuts (left sidebar).
 * @details Width is not touched here — the sidebar splitter owns it, and this
 * runs on every open()/save() so resetting would undo the user's drag. */
inline void installFileBrowserQuickAccess(ImGui::FileBrowser& browser) {
	namespace fs = std::filesystem;
	browser.ClearQuickAccess();

	auto addIfDir = [&](const char* label, const fs::path& p) {
		if (!p.empty()) {
			browser.AddQuickAccess(label, p);
		}
	};

	// The running app's data folder first — it is where app documents live.
	const std::string data = AppPaths::getDataDir();
	if (!data.empty()) {
		addIfDir("App data", fs::path(data));
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
 * @brief Apply extension filters and always offer `.*` (all files) in the combo.
 * @details imgui-filebrowser uses `.*` for any extension (not Windows `*.*`).
 * Callers pass preferred types first; `.*` is appended when missing so the
 * directory listing can still show everything. Also installs the quick-access
 * sidebar when filters are applied.
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
	installFileBrowserQuickAccess(browser);
}

/**
 * @brief Open/save browsers for Mui — keeps ImGui::FileBrowser out of IMui.
 * @details Window size uses design constants × content scale (same story as
 * notification width), so HiDPI does not leave a tiny browser.
 */
class FileDialogs {
  public:
	using Callback = std::function<void(const std::string& path)>;

	FileDialogs();

	/**
	 * @brief Apply GLFW/ImGui content scale to dialog layout.
	 * @param scale Window content scale; values below 0.5 are treated as 1.
	 */
	void setDpiScale(float scale);

	void open(const std::string& title, std::vector<std::string> filters, Callback onSelected);
	void save(const std::string& title, std::vector<std::string> filters, Callback onSelected);

	/** @brief Draw browsers and fire callback when a path is chosen. */
	void tick();

  private:
	void applyLayout();

	enum class Mode { None, Open, Save };

	ImGui::FileBrowser m_open;
	ImGui::FileBrowser m_save;
	Mode m_mode = Mode::None;
	Callback m_callback;
	float m_dpiScale = 1.f;
};

} // namespace rigkit
