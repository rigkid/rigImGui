#include "FileDialogs.h"

#include <GLFW/glfw3.h>

#include <cctype>

namespace rigkit {
namespace {

std::vector<std::filesystem::path> g_osDrops;

std::filesystem::path pathFromUtf8(const char* text) {
	const std::string bytes(text);
	return std::filesystem::path(std::u8string(bytes.begin(), bytes.end()));
}

void osDropCallback(GLFWwindow*, int count, const char** paths) {
	if (!paths) {
		return;
	}
	for (int i = 0; i < count; ++i) {
		if (paths[i] && paths[i][0]) {
			g_osDrops.push_back(pathFromUtf8(paths[i]));
		}
	}
}

std::vector<std::filesystem::path> takeOsDrops() {
	std::vector<std::filesystem::path> out;
	out.swap(g_osDrops);
	return out;
}

// Explorer drag often moves the cursor without ImGui marking a window hovered.
// Trust the window rectangle when nothing else is hovered.
bool cursorOverFileDialog() {
	const ImGuiHoveredFlags flags =
		ImGuiHoveredFlags_RootAndChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem;
	if (ImGui::IsWindowHovered(flags)) {
		return true;
	}
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const ImVec2 pos = ImGui::GetWindowPos();
	const ImVec2 size = ImGui::GetWindowSize();
	const bool inside = mouse.x >= pos.x && mouse.y >= pos.y && mouse.x < pos.x + size.x &&
						mouse.y < pos.y + size.y;
	return inside && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
}

bool isConcreteExtension(const std::string& f) {
	return f.size() >= 2 && f[0] == '.' && f != ".*" && f.find(',') == std::string::npos;
}

bool endsWithIgnoreCase(const std::string& s, const std::string& suffix) {
	if (s.size() < suffix.size()) {
		return false;
	}
	for (size_t i = 0; i < suffix.size(); ++i) {
		const unsigned char a = static_cast<unsigned char>(s[s.size() - suffix.size() + i]);
		const unsigned char b = static_cast<unsigned char>(suffix[i]);
		if (std::tolower(a) != std::tolower(b)) {
			return false;
		}
	}
	return true;
}

ImGuiFileBrowserFlags openFlags() {
	return ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_ConfirmOnEnter |
		   ImGuiFileBrowserFlags_SkipItemsCausingError | ImGuiFileBrowserFlags_EditPathString;
}

ImGuiFileBrowserFlags saveFlags() {
	return ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir |
		   ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_ConfirmOnEnter |
		   ImGuiFileBrowserFlags_SkipItemsCausingError | ImGuiFileBrowserFlags_EditPathString;
}

ImGuiFileBrowserFlags folderFlags() {
	return ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_HideRegularFiles |
		   ImGuiFileBrowserFlags_CreateNewDir | ImGuiFileBrowserFlags_CloseOnEsc |
		   ImGuiFileBrowserFlags_ConfirmOnEnter | ImGuiFileBrowserFlags_SkipItemsCausingError |
		   ImGuiFileBrowserFlags_EditPathString;
}

} // namespace

FileDialogWindow::FileDialogWindow()
	: ImGuiPanel("File###FileDialog", ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse) {
	setVisible(false);
}

void FileDialogWindow::setBrowser(ImGui::FileBrowser* browser) { m_browser = browser; }

void FileDialogWindow::offerDroppedPath(std::filesystem::path path) {
	m_droppedPath = std::move(path);
}

bool FileDialogWindow::drewContents() const { return m_drewContents; }

void FileDialogWindow::render() {
	m_drewContents = false;
	if (!isVisible()) {
		m_droppedPath.clear();
		return;
	}
	int w = 0;
	int h = 0;
	uiWindowSize(kFileDialogDesignW, kFileDialogDesignH, w, h);
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), static_cast<float>(h)),
							 ImGuiCond_Appearing);
	if (const ImGuiViewport* vp = ImGui::GetMainViewport()) {
		ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	}
	ImGuiPanel::render();
	m_droppedPath.clear();
}

void FileDialogWindow::renderContents() {
	m_drewContents = true;
	if (m_browser && !m_droppedPath.empty() && cursorOverFileDialog()) {
		m_browser->RevealPath(m_droppedPath);
	}
	m_droppedPath.clear();
	if (m_browser) {
		m_browser->DrawContents();
	}
}

FileDialogs::FileDialogs() : m_open(openFlags()), m_save(saveFlags()), m_folder(folderFlags()) {
	// 1x until open()/save() (ImGui context + FontScaleDpi ready).
	applyFileBrowserLayout(m_open);
	applyFileBrowserLayout(m_save);
	applyFileBrowserLayout(m_folder);
	installFileBrowserQuickAccess(m_open);
	installFileBrowserQuickAccess(m_save);
	installFileBrowserQuickAccess(m_folder);
	const auto thumbs = [this](const std::filesystem::path& path) {
		ImGui::FileBrowser::FileThumbnail out;
		bool pending = false;
		out.id = m_thumbs.get(path, &out.w, &out.h, &pending);
		out.pending = pending;
		out.paused = m_thumbs.paused();
		return out;
	};
	const auto cancelThumb = [this](const std::filesystem::path& path) { m_thumbs.cancel(path); };
	const auto thumbPaused = [this]() { return m_thumbs.paused(); };
	const auto toggleThumbPause = [this]() { m_thumbs.setPaused(!m_thumbs.paused()); };
	m_open.SetThumbnailProvider(thumbs);
	m_save.SetThumbnailProvider(thumbs);
	m_open.SetThumbnailCancel(cancelThumb);
	m_save.SetThumbnailCancel(cancelThumb);
	m_open.SetThumbnailPause(thumbPaused, toggleThumbPause);
	m_save.SetThumbnailPause(thumbPaused, toggleThumbPause);
	m_open.SetWindowHosted(true);
	m_save.SetWindowHosted(true);
	m_folder.SetWindowHosted(true);
}

FileDialogs::~FileDialogs() { shutdown(); }

void FileDialogs::shutdown() { m_thumbs.shutdown(); }

void FileDialogs::applyLayout() {
	applyFileBrowserLayout(m_open);
	applyFileBrowserLayout(m_save);
	applyFileBrowserLayout(m_folder);
}

void FileDialogs::showOpen(const std::string& title, std::vector<std::string> filters, bool many) {
	applyLayout();
	clearOverwriteConfirm();
	m_mode = Mode::Open;
	m_multi = many;
	m_open.SetMultipleSelection(many);
	const std::string label = title.empty() ? "Open" : title;
	m_open.SetTitle(label);
	setFileBrowserFilters(m_open, std::move(filters), true);
	m_window.setTitle(label + "###FileDialog");
	m_open.Open();
	m_window.setVisible(true);
}

void FileDialogs::open(const std::string& title, std::vector<std::string> filters,
					   Callback onSelected) {
	m_multiCallback = nullptr;
	m_callback = std::move(onSelected);
	showOpen(title, std::move(filters), false);
}

void FileDialogs::openMany(const std::string& title, std::vector<std::string> filters,
						   MultiCallback onSelected) {
	m_callback = nullptr;
	m_multiCallback = std::move(onSelected);
	showOpen(title, std::move(filters), true);
}

void FileDialogs::clearOverwriteConfirm() {
	m_confirmOverwrite = false;
	m_overwriteCallback = nullptr;
	m_overwritePath.clear();
}

void FileDialogs::save(const std::string& title, std::vector<std::string> filters,
					   Callback onSelected, const std::string& defaultName) {
	applyLayout();
	clearOverwriteConfirm();
	m_mode = Mode::Save;
	m_multi = false;
	m_multiCallback = nullptr;
	m_callback = std::move(onSelected);
	const std::string label = title.empty() ? "Save" : title;
	m_save.SetTitle(label);
	setFileBrowserFilters(m_save, std::move(filters));
	if (!defaultName.empty()) {
		m_save.SetInputName(defaultName);
	}
	m_window.setTitle(label + "###FileDialog");
	m_save.Open();
	m_window.setVisible(true);
}

void FileDialogs::openFolder(const std::string& title, Callback onSelected,
							 const std::string& startDir) {
	applyLayout();
	clearOverwriteConfirm();
	m_mode = Mode::Folder;
	m_multi = false;
	m_multiCallback = nullptr;
	m_callback = std::move(onSelected);
	const std::string label = title.empty() ? "Choose folder" : title;
	m_folder.SetTitle(label);
	if (!startDir.empty()) {
		std::error_code ec;
		const std::filesystem::path dir(std::u8string(startDir.begin(), startDir.end()));
		if (std::filesystem::is_directory(dir, ec)) {
			m_folder.SetDirectory(dir);
		}
	}
	m_window.setTitle(label + "###FileDialog");
	m_folder.Open();
	m_window.setVisible(true);
}

std::string FileDialogs::withSaveExtension(std::filesystem::path path) const {
	// Same as native Save dialogs: append only when the type combo is a real
	// extension. ".*" (All files) means leave the stem alone. Never double-append
	// (file.svg + .svg must stay file.svg, not file.svg.svg).
	const std::string& ext = m_save.GetCurrentTypeFilter();
	if (!isConcreteExtension(ext)) {
		return path.string();
	}
	if (endsWithIgnoreCase(path.filename().string(), ext)) {
		return path.string();
	}
	path += ext;
	return path.string();
}

void FileDialogs::installDropTarget(GLFWwindow* window) {
	if (window) {
		glfwSetDropCallback(window, osDropCallback);
	}
	if (!ImGui::GetCurrentContext()) {
		return;
	}
	ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
	for (int i = 0; i < platform.Viewports.Size; ++i) {
		ImGuiViewport* viewport = platform.Viewports[i];
		if (!viewport || !viewport->PlatformHandle) {
			continue;
		}
		glfwSetDropCallback(static_cast<GLFWwindow*>(viewport->PlatformHandle), osDropCallback);
	}
}

void FileDialogs::setUnclaimedFileDropHandler(DropHandler handler) {
	m_unclaimedDrop = std::move(handler);
}

void FileDialogs::flushPendingSelection() {
	if (m_pendingMultiCallback) {
		auto cb = std::move(m_pendingMultiCallback);
		auto paths = std::move(m_pendingPaths);
		m_pendingCallback = nullptr;
		m_pendingPath.clear();
		cb(paths);
		return;
	}
	if (m_pendingCallback) {
		auto cb = std::move(m_pendingCallback);
		auto path = std::move(m_pendingPath);
		m_pendingPaths.clear();
		cb(path);
	}
}

void FileDialogs::renderOverwriteConfirm() {
	if (!m_confirmOverwrite) {
		return;
	}
	const ImGuiViewport* vp = ImGui::GetMainViewport();
	if (vp) {
		ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	}
	ImGui::OpenPopup("##OverwriteFile");
	if (!ImGui::BeginPopupModal("##OverwriteFile", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		return;
	}
	ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + uiPx(400.f));
	ImGui::TextUnformatted("Are you sure you want to overwrite this file?");
	ImGui::Spacing();
	const std::string name = std::filesystem::path(m_overwritePath).filename().string();
	ImGui::TextUnformatted(name.empty() ? m_overwritePath.c_str() : name.c_str());
	ImGui::PopTextWrapPos();
	ImGui::Separator();
	const float btnW = uiPx(120.f);
	if (ImGui::Button("Overwrite", ImVec2(btnW, 0.f))) {
		m_pendingCallback = std::move(m_overwriteCallback);
		m_pendingPath = std::move(m_overwritePath);
		m_pendingMultiCallback = nullptr;
		m_pendingPaths.clear();
		clearOverwriteConfirm();
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(btnW, 0.f))) {
		clearOverwriteConfirm();
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void FileDialogs::tick() {
	// Flush thumbs doomed last frame after ImGui has already rendered them.
	std::vector<std::filesystem::path> dropped = takeOsDrops();
	m_thumbs.tick();
	renderOverwriteConfirm();
	if (m_mode == Mode::None) {
		if (m_window.isVisible()) {
			m_window.setVisible(false);
		}
		// Hold the write until Overwrite is chosen - Cancel drops the path.
		if (!m_confirmOverwrite) {
			// Run open/save after the dialog has torn down so a heavy load cannot
			// stall the closing frame (looks like a hang after picking a file).
			flushPendingSelection();
		}
		if (m_unclaimedDrop && !dropped.empty()) {
			std::vector<std::string> paths;
			paths.reserve(dropped.size());
			for (const auto& path : dropped) {
				paths.push_back(path.string());
			}
			m_unclaimedDrop(paths);
		}
		m_thumbs.finishFrame();
		return;
	}

	ImGui::FileBrowser& browser = m_mode == Mode::Save	   ? m_save
								  : m_mode == Mode::Folder ? m_folder
														   : m_open;
	m_window.setBrowser(&browser);
	if (!dropped.empty()) {
		m_window.offerDroppedPath(std::move(dropped.front()));
	}
	m_window.render();

	if (browser.ConsumeListingReload()) {
		m_thumbs.clear();
	}

	if (browser.ConsumePinnedQuickAccessChanged()) {
		const auto pins = browser.GetPinnedQuickAccess();
		if (&browser != &m_open) {
			m_open.SetPinnedQuickAccess(pins);
		}
		if (&browser != &m_save) {
			m_save.SetPinnedQuickAccess(pins);
		}
		if (&browser != &m_folder) {
			m_folder.SetPinnedQuickAccess(pins);
		}
		m_pinsChanged = true;
	}
	if (browser.ConsumeViewModeChanged()) {
		const auto mode = browser.GetViewMode();
		if (&browser != &m_open) {
			m_open.SetViewMode(mode);
		}
		if (&browser != &m_save) {
			m_save.SetViewMode(mode);
		}
		if (&browser != &m_folder) {
			m_folder.SetViewMode(mode);
		}
		m_viewChanged = true;
	}
	if (browser.ConsumeShowHiddenChanged()) {
		const bool show = browser.GetShowHidden();
		if (&browser != &m_open) {
			m_open.SetShowHidden(show);
		}
		if (&browser != &m_save) {
			m_save.SetShowHidden(show);
		}
		if (&browser != &m_folder) {
			m_folder.SetShowHidden(show);
		}
		m_hiddenChanged = true;
	}

	if (browser.HasSelected()) {
		std::vector<std::string> many;
		std::string path;
		if (m_mode == Mode::Open && m_multi) {
			for (const auto& picked : browser.GetMultiSelected()) {
				many.push_back(picked.string());
			}
		} else {
			path = browser.GetSelected().string();
			if (m_mode == Mode::Save) {
				path = withSaveExtension(browser.GetSelected());
			}
		}
		browser.ClearSelected();
		const bool saveReplace = m_mode == Mode::Save && [&]() {
			std::error_code ec;
			return !path.empty() && std::filesystem::exists(path, ec);
		}();
		if (saveReplace) {
			m_overwriteCallback = std::move(m_callback);
			m_overwritePath = std::move(path);
			m_confirmOverwrite = true;
			m_callback = nullptr;
			m_multiCallback = nullptr;
			m_multi = false;
			m_mode = Mode::None;
			m_window.setVisible(false);
			m_thumbs.clear();
		} else {
			m_pendingCallback = std::move(m_callback);
			m_pendingMultiCallback = std::move(m_multiCallback);
			m_pendingPath = std::move(path);
			m_pendingPaths = std::move(many);
			m_callback = nullptr;
			m_multiCallback = nullptr;
			m_multi = false;
			m_mode = Mode::None;
			m_window.setVisible(false);
			m_thumbs.clear();
		}
	} else if (!m_window.isVisible() || (m_window.drewContents() && !browser.IsOpened())) {
		m_callback = nullptr;
		m_multiCallback = nullptr;
		m_multi = false;
		m_mode = Mode::None;
		m_window.setVisible(false);
		m_thumbs.clear();
	}
	m_thumbs.finishFrame();
}

void FileDialogs::setPinnedFolders(const std::vector<std::string>& paths) {
	std::vector<std::filesystem::path> fsPaths;
	fsPaths.reserve(paths.size());
	for (const auto& p : paths) {
		if (p.empty()) {
			continue;
		}
		// UTF-8 bytes to path (fs::u8path is deprecated in C++20).
		fsPaths.emplace_back(std::u8string(p.begin(), p.end()));
	}
	m_open.SetPinnedQuickAccess(fsPaths);
	m_save.SetPinnedQuickAccess(fsPaths);
	m_folder.SetPinnedQuickAccess(fsPaths);
}

std::vector<std::string> FileDialogs::pinnedFolders() const {
	std::vector<std::string> out;
	for (const auto& p : m_open.GetPinnedQuickAccess()) {
		const std::u8string u8 = p.u8string();
		out.emplace_back(u8.begin(), u8.end());
	}
	return out;
}

bool FileDialogs::consumePinnedFoldersChanged() {
	const bool changed = m_pinsChanged;
	m_pinsChanged = false;
	return changed;
}

void FileDialogs::setViewMode(ImGui::FileBrowser::ViewMode mode) {
	m_open.SetViewMode(mode);
	m_save.SetViewMode(mode);
	m_folder.SetViewMode(mode);
}

ImGui::FileBrowser::ViewMode FileDialogs::viewMode() const { return m_open.GetViewMode(); }

bool FileDialogs::consumeViewModeChanged() {
	const bool changed = m_viewChanged;
	m_viewChanged = false;
	return changed;
}

void FileDialogs::setShowHidden(bool show) {
	m_open.SetShowHidden(show);
	m_save.SetShowHidden(show);
	m_folder.SetShowHidden(show);
}

bool FileDialogs::showHidden() const { return m_open.GetShowHidden(); }

bool FileDialogs::consumeShowHiddenChanged() {
	const bool changed = m_hiddenChanged;
	m_hiddenChanged = false;
	return changed;
}

void FileDialogs::registerThumbnailDecoder(const std::string& extension,
										   IMui::FileThumbnailDecoder decoder) {
	m_thumbs.registerDecoder(extension, std::move(decoder));
}

} // namespace rigkit
