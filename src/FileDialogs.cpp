#include "FileDialogs.h"

namespace rigkit {

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

void FileDialogs::tick() {
	if (m_mode == Mode::None) {
		return;
	}

	ImGui::FileBrowser& browser = (m_mode == Mode::Open) ? m_open : m_save;
	browser.Display();

	if (browser.HasSelected()) {
		const std::string path = browser.GetSelected().string();
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
