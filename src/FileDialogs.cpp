#include "FileDialogs.h"

#include <cctype>

namespace rigkit {
namespace {

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

} // namespace

FileDialogs::FileDialogs()
	: m_open(ImGuiFileBrowserFlags_CloseOnEsc),
	  m_save(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_CreateNewDir |
			 ImGuiFileBrowserFlags_CloseOnEsc) {
	// 1x until open()/save() (ImGui context + FontScaleDpi ready).
	applyFileBrowserLayout(m_open);
	applyFileBrowserLayout(m_save);
	installFileBrowserQuickAccess(m_open);
	installFileBrowserQuickAccess(m_save);
}

void FileDialogs::applyLayout() {
	applyFileBrowserLayout(m_open);
	applyFileBrowserLayout(m_save);
}

void FileDialogs::open(const std::string& title, std::vector<std::string> filters,
					   Callback onSelected) {
	applyLayout();
	m_mode = Mode::Open;
	m_callback = std::move(onSelected);
	m_open.SetTitle(title.empty() ? "Open" : title);
	setFileBrowserFilters(m_open, std::move(filters));
	installFileBrowserQuickAccess(m_open);
	m_open.Open();
}

void FileDialogs::save(const std::string& title, std::vector<std::string> filters,
					   Callback onSelected) {
	applyLayout();
	m_mode = Mode::Save;
	m_callback = std::move(onSelected);
	m_save.SetTitle(title.empty() ? "Save" : title);
	setFileBrowserFilters(m_save, std::move(filters));
	installFileBrowserQuickAccess(m_save);
	m_save.Open();
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

void FileDialogs::tick() {
	if (m_mode == Mode::None) {
		return;
	}

	ImGui::FileBrowser& browser = (m_mode == Mode::Open) ? m_open : m_save;
	browser.Display();

	if (browser.HasSelected()) {
		std::string path = browser.GetSelected().string();
		if (m_mode == Mode::Save) {
			path = withSaveExtension(browser.GetSelected());
		}
		browser.ClearSelected();
		auto cb = std::move(m_callback);
		m_callback = nullptr;
		m_mode = Mode::None;
		if (cb) {
			cb(path);
		}
	} else if (!browser.IsOpened()) {
		m_callback = nullptr;
		m_mode = Mode::None;
	}
}

} // namespace rigkit
