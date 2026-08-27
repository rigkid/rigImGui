#include "HostMenuBar.h"

#include "core/RigKitEngine.h"
#include "core/util/UndoStack.h"
#include "ecs/MEcs.h"
#include "rendering/U_gladGlfw.h"
#include "CCamera.h"
#include "ImGuiStyleKit.h"
#include "Mui.h"
#include "MWindow.h"
#include "UiDpi.h"

#include <filesystem>
#include <imgui.h>
#include <string>

namespace rigkit {

HostMenuBar::HostMenuBar(Mui *ui) : m_ui(ui) {}

void HostMenuBar::render() {
	if (!ImGui::BeginMainMenuBar()) {
		return;
	}

	const ImGuiStyle &style = ImGui::GetStyle();
	// BeginMenu adds ItemSpacing/2; File glyphs land on chromeBarPadX().
	const float padX = chromeMenuBarCursorX();
	ImGui::SetCursorPosX(padX);

	renderAppMenu();
	renderFileMenu();
	renderEditMenu();
	renderViewMenu();
	renderToolsMenu();
	renderHelpMenu();

	if (m_ui) {
		std::string status;
		const int fpsMode =
			(m_ui->uiPrefs().fpsDisplay < 0 || m_ui->uiPrefs().fpsDisplay > 2)
				? 0
				: m_ui->uiPrefs().fpsDisplay;
		if (fpsMode == 1) {
			status = m_ui->fpsStatusText();
		}
		const auto &statusFn = m_ui->menuBarRightStatus();
		if (statusFn) {
			const std::string appStatus = statusFn();
			if (!appStatus.empty()) {
				if (!status.empty()) {
					status += "  |  ";
				}
				status += appStatus;
			}
		}
		if (!status.empty()) {
			const float textW = ImGui::CalcTextSize(status.c_str()).x;
			// Unframed text: extra FramePadding so glyphs sit like a menu/button label.
			const float padRight = padX + style.FramePadding.x;
			ImGui::SameLine(ImGui::GetWindowWidth() - textW - padRight);
			ImGui::TextDisabled("%s", status.c_str());
		}
	}

	ImGui::EndMainMenuBar();

	// Popups cannot open inside a closing menu; open at root level instead.
	if (m_openWorkspaceSavePopup) {
		m_openWorkspaceSavePopup = false;
		m_workspaceNameBuf[0] = '\0';
		ImGui::OpenPopup("Save Workspace As");
	}
	renderWorkspaceSavePopup();
}

void HostMenuBar::renderWorkspaceMenu() {
	if (!ImGui::BeginMenu("Workspace")) {
		return;
	}
	if (!m_ui) {
		ImGui::TextDisabled("UI manager unavailable");
		ImGui::EndMenu();
		return;
	}
	const auto names = m_ui->workspaceNames();
	const std::string &current = m_ui->currentWorkspace();
	for (const auto &name : names) {
		if (ImGui::MenuItem(name.c_str(), nullptr, name == current)) {
			m_ui->loadWorkspace(name);
		}
	}
	if (names.empty()) {
		ImGui::TextDisabled("No saved workspaces");
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Save Workspace As...")) {
		m_openWorkspaceSavePopup = true;
	}
	bool canDelete = false;
	for (const auto &name : names) {
		if (name != Mui::kStandardWorkspace) {
			canDelete = true;
			break;
		}
	}
	if (ImGui::BeginMenu("Delete Workspace", canDelete)) {
		for (const auto &name : names) {
			if (name == Mui::kStandardWorkspace) {
				continue;
			}
			if (ImGui::MenuItem(name.c_str())) {
				Mui *ui = m_ui;
				ui->showModal("Delete Workspace",
							  "Delete workspace \"" + name + "\"?",
							  NotificationType::Warning,
							  [ui, name]() { ui->deleteWorkspace(name); });
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndMenu();
}

void HostMenuBar::renderWorkspaceSavePopup() {
	if (!m_ui) {
		return;
	}
	const ImGuiViewport *vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (!ImGui::BeginPopupModal("Save Workspace As", nullptr,
								ImGuiWindowFlags_AlwaysAutoResize)) {
		return;
	}
	ImGui::TextUnformatted("Workspace name");
	if (ImGui::IsWindowAppearing()) {
		ImGui::SetKeyboardFocusHere();
	}
	const bool submitted =
		ImGui::InputText("##workspace_name", m_workspaceNameBuf, sizeof(m_workspaceNameBuf),
						 ImGuiInputTextFlags_EnterReturnsTrue);
	const bool hasName = m_workspaceNameBuf[0] != '\0';
	ImGui::BeginDisabled(!hasName);
	if ((ImGui::Button("Save") || submitted) && hasName) {
		if (m_ui->saveWorkspace(m_workspaceNameBuf)) {
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Cancel")) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void HostMenuBar::renderAppMenu() {
	const char *appLabel = "App";
	if (m_engine && m_engine->getApp()) {
		const auto &name = m_engine->getApp()->settings().appName;
		if (!name.empty()) {
			appLabel = name.c_str();
		}
	}
	if (!ImGui::BeginMenu(appLabel)) {
		return;
	}
	if (m_ui) {
		const auto &rows = m_ui->appActions();
		for (const auto &row : rows) {
			if (row.submenu) {
				if (ImGui::BeginMenu(row.label.c_str())) {
					if (row.action) {
						row.action();
					}
					ImGui::EndMenu();
				}
				continue;
			}
			const char *chord = row.shortcut.empty() ? nullptr : row.shortcut.c_str();
			if (ImGui::MenuItem(row.label.c_str(), chord)) {
				if (row.action) {
					row.action();
				}
			}
		}
		if (!rows.empty()) {
			ImGui::Separator();
		}
	}
	renderWorkspaceMenu();
	if (ImGui::MenuItem("Preferences...")) {
		if (m_ui) {
			m_ui->showPreferences();
		}
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Quit", "Alt+F4")) {
		if (m_engine && m_engine->getWindow()) {
			glfwSetWindowShouldClose(m_engine->getWindow(), GLFW_TRUE);
		}
	}
	ImGui::EndMenu();
}

void HostMenuBar::renderFileMenu() {
	if (!ImGui::BeginMenu("File")) {
		return;
	}
	if (m_ui) {
		const auto &rows = m_ui->fileActions();
		for (const auto &row : rows) {
			if (row.submenu) {
				if (ImGui::BeginMenu(row.label.c_str())) {
					if (row.action) {
						row.action();
					}
					ImGui::EndMenu();
				}
				continue;
			}
			const char *chord = row.shortcut.empty() ? nullptr : row.shortcut.c_str();
			if (ImGui::MenuItem(row.label.c_str(), chord)) {
				if (row.action) {
					row.action();
				}
			}
		}
		if (!rows.empty()) {
			ImGui::Separator();
		}

		const auto &recent = m_ui->recentFiles();
		if (ImGui::BeginMenu("Open Recent", !recent.empty())) {
			for (size_t i = 0; i < recent.size(); ++i) {
				const std::string &path = recent[i];
				std::string label;
				try {
					label = std::filesystem::path(path).filename().string();
				} catch (...) {
					label.clear();
				}
				if (label.empty()) {
					label = path;
				}
				// Unique id when basenames collide; tooltip shows the full path.
				const std::string item = label + "##recent_" + std::to_string(i);
				if (ImGui::MenuItem(item.c_str())) {
					if (const auto &handler = m_ui->recentFileOpenHandler()) {
						handler(path);
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s", path.c_str());
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Clear Recent", nullptr, false, !recent.empty())) {
				m_ui->clearRecentFiles();
			}
			ImGui::EndMenu();
		}
	}
	if (ImGui::BeginMenu("Export")) {
		if (ImGui::MenuItem("PNG (framebuffer)…")) {
			if (m_ui) {
				m_ui->requestExportPng();
			}
		}
		if (m_ui) {
			for (const auto &row : m_ui->exportActions()) {
				if (ImGui::MenuItem(row.first.c_str())) {
					if (row.second) {
						row.second();
					}
				}
			}
		}
		ImGui::EndMenu();
	}
	ImGui::EndMenu();
}

void HostMenuBar::renderEditMenu() {
	if (!ImGui::BeginMenu("Edit")) {
		return;
	}

	UndoStack *stack = m_ui ? m_ui->undoStack() : nullptr;
	std::string undoLabel = "Undo";
	std::string redoLabel = "Redo";
	if (stack && stack->canUndo() && !stack->undoLabel().empty()) {
		undoLabel = "Undo " + stack->undoLabel();
	}
	if (stack && stack->canRedo() && !stack->redoLabel().empty()) {
		redoLabel = "Redo " + stack->redoLabel();
	}

	const char* undoChord = "Ctrl+Z";
	const char* redoChord = "Ctrl+Y";
	const char* editModeChord = "Ctrl+E";
	std::string undoChordOwned;
	std::string redoChordOwned;
	std::string editModeChordOwned;
	if (m_ui) {
		undoChordOwned = m_ui->shortcuts().chordLabelFor("edit.undo");
		redoChordOwned = m_ui->shortcuts().chordLabelFor("edit.redo");
		editModeChordOwned = m_ui->shortcuts().chordLabelFor("view.edit_mode");
		if (!undoChordOwned.empty()) {
			undoChord = undoChordOwned.c_str();
		}
		if (!redoChordOwned.empty()) {
			redoChord = redoChordOwned.c_str();
		}
		if (!editModeChordOwned.empty()) {
			editModeChord = editModeChordOwned.c_str();
		}
	}

	if (ImGui::MenuItem(undoLabel.c_str(), undoChord, false, stack && stack->canUndo())) {
		stack->undo();
	}
	if (ImGui::MenuItem(redoLabel.c_str(), redoChord, false, stack && stack->canRedo())) {
		stack->redo();
	}

	if (m_ui && !m_ui->editActions().empty()) {
		ImGui::Separator();
		for (const auto &row : m_ui->editActions()) {
			const bool enabled = !row.isEnabled || row.isEnabled();
			const char *chord = row.shortcut.empty() ? nullptr : row.shortcut.c_str();
			if (ImGui::MenuItem(row.label.c_str(), chord, false, enabled) && row.action) {
				row.action();
			}
		}
	}

	if (m_ui && m_ui->editModeEnabled()) {
		ImGui::Separator();
		const bool editMode = m_ui->editMode();
		if (ImGui::MenuItem("Edit Mode", editModeChord, editMode)) {
			m_ui->setEditMode(!editMode);
		}
	}

	ImGui::EndMenu();
}

void HostMenuBar::renderViewMenu() {
	if (!ImGui::BeginMenu("View")) {
		return;
	}

	if (m_ui) {
		bool rulers = m_ui->rulersVisible();
		if (ImGui::MenuItem("Rulers", "F2", rulers)) {
			m_ui->setRulersVisible(!rulers);
		}
		bool handles = m_ui->handles2DVisible();
		if (ImGui::MenuItem("2D Handles", nullptr, handles)) {
			m_ui->setHandles2DVisible(!handles);
		}
		ImGui::Separator();
	}

	if (m_engine && m_engine->getECSManager()) {
		auto* ecs = m_engine->getECSManager();
		ecs::CCamera* cam = nullptr;
		auto cameras = ecs->view<ecs::CCamera>();
		for (auto entity : cameras) {
			auto& c = cameras.get<ecs::CCamera>(entity);
			if (c.active) {
				cam = &c;
				break;
			}
		}
		if (cam && ImGui::BeginMenu("Shade")) {
			using Shade = ecs::CCamera::Shade;
			if (ImGui::MenuItem("Solid", nullptr, cam->shade == Shade::Solid)) {
				cam->shade = Shade::Solid;
			}
			if (ImGui::MenuItem("Wireframe", nullptr, cam->shade == Shade::Wireframe)) {
				cam->shade = Shade::Wireframe;
			}
			if (ImGui::MenuItem("Sketch", nullptr, cam->shade == Shade::Sketch)) {
				cam->shade = Shade::Sketch;
			}
			ImGui::EndMenu();
		}
	}

	if (m_ui && !m_ui->viewActions().empty()) {
		ImGui::Separator();
		for (const auto &row : m_ui->viewActions()) {
			if (row.submenu) {
				if (ImGui::BeginMenu(row.label.c_str())) {
					if (row.action) {
						row.action();
					}
					ImGui::EndMenu();
				}
			} else {
				const char *chord = row.shortcut.empty() ? nullptr : row.shortcut.c_str();
				if (ImGui::MenuItem(row.label.c_str(), chord) && row.action) {
					row.action();
				}
			}
		}
	}

	if (ImGui::BeginMenu("Windows")) {
		if (m_ui && m_ui->getWindowManager()) {
			auto *wm = m_ui->getWindowManager();
			auto names = wm->getAllWindowNames();
			for (const auto &name : names) {
				bool visible = wm->getWindowVisibility(name);
				if (ImGui::MenuItem(name.c_str(), nullptr, visible)) {
					wm->setWindowVisible(name, !visible);
				}
			}
			if (names.empty()) {
				ImGui::TextDisabled("No windows");
			}
		} else {
			ImGui::TextDisabled("Window manager unavailable");
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Theme")) {
		if (m_ui) {
			const int current = static_cast<int>(m_ui->getImGuiTheme());
			if (ImGui::MenuItem("Dark", nullptr, current == 0)) {
				m_ui->setImGuiTheme(ImGuiTheme::Dark);
			}
			if (ImGui::MenuItem("Light", nullptr, current == 1)) {
				m_ui->setImGuiTheme(ImGuiTheme::Light);
			}
			ImGui::Separator();
			const std::string& scheme = m_ui->uiPrefs().themeFile;
			if (ImGui::MenuItem("None", nullptr, scheme.empty())) {
				m_ui->uiPrefs().themeFile.clear();
				m_ui->setImGuiTheme(m_ui->getImGuiTheme());
			}
			for (const auto& f : ImGuiStyleKit::listThemeFiles()) {
				const bool sel = scheme == f.fileName || scheme == f.path;
				if (ImGui::MenuItem(f.name.c_str(), nullptr, sel)) {
					m_ui->uiPrefs().themeFile = f.fileName;
					m_ui->setImGuiTheme(m_ui->getImGuiTheme());
				}
			}
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Chrome")) {
		const bool isImgui =
			!m_engine || m_engine->uiChrome() == "imgui" || m_engine->uiChrome().empty();
		const bool isTui = m_engine && m_engine->uiChrome() == "tui";
		if (ImGui::MenuItem("ImGui", nullptr, isImgui)) {
			if (m_engine) {
				m_engine->requestUiChrome("imgui");
			}
		}
		if (ImGui::MenuItem("ImTui", nullptr, isTui)) {
			if (m_engine) {
				m_engine->requestUiChrome("tui");
			}
		}
		ImGui::EndMenu();
	}

	ImGui::EndMenu();
}

void HostMenuBar::renderToolsMenu() {
	if (!ImGui::BeginMenu("Tools")) {
		return;
	}
	if (!m_ui) {
		ImGui::EndMenu();
		return;
	}
	const auto &toolRows = m_ui->toolActions();
	if (!toolRows.empty()) {
		for (const auto &row : toolRows) {
			const bool active = row.isActive ? row.isActive() : false;
			const char *chord = row.shortcut.empty() ? nullptr : row.shortcut.c_str();
			if (ImGui::MenuItem(row.label.c_str(), chord, active)) {
				if (row.action) {
					row.action();
				}
			}
		}
		ImGui::EndMenu();
		return;
	}
	const auto op = m_ui->gizmoOp();
	if (ImGui::MenuItem("Select", "V", op == IMui::GizmoOp::Select)) {
		m_ui->setGizmoOp(IMui::GizmoOp::Select);
	}
	if (ImGui::MenuItem("Move", "W", op == IMui::GizmoOp::Translate)) {
		m_ui->setGizmoOp(IMui::GizmoOp::Translate);
	}
	if (ImGui::MenuItem("Rotate", "E", op == IMui::GizmoOp::Rotate)) {
		m_ui->setGizmoOp(IMui::GizmoOp::Rotate);
	}
	if (ImGui::MenuItem("Scale", "R", op == IMui::GizmoOp::Scale)) {
		m_ui->setGizmoOp(IMui::GizmoOp::Scale);
	}
	ImGui::EndMenu();
}

void HostMenuBar::renderHelpMenu() {
	if (!ImGui::BeginMenu("Help")) {
		return;
	}
	if (ImGui::MenuItem("About...")) {
		if (m_ui) {
			m_ui->showAbout();
		}
	}
	ImGui::EndMenu();
}

} // namespace rigkit
