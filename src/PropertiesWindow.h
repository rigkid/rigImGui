#pragma once

#include <functional>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include "IWindow.h"
#include "ecs/MEcs.h"

namespace rigkit {

class MSettings;

class PropertiesWindow : public IWindow {
  public:
	using ExtraDrawer = std::function<void(MEcs& ecs, entt::entity entity)>;
	/** @brief Optional — when set, Edit shows "Open in Code Editor" for `CCode`. */
	using OpenCodeEditorFn = std::function<void(uint32_t entity)>;
	/**
	 * @brief Optional highlighted light editor (e.g. from rigCodeEditor).
	 * @return true when @p text was modified.
	 */
	using CodeLightEditDrawFn =
		std::function<bool(uint32_t entity, std::string& text, const std::string& language,
						   float height, bool readOnly)>;

	PropertiesWindow(const std::string& title = "Properties", ImGuiWindowFlags flags = 0);
	virtual ~PropertiesWindow() = default;

	void setSelectedEntity(uint32_t entity);
	uint32_t getSelectedEntity() const;

	void setOnEntitySelected(std::function<void(uint32_t)> callback);
	void setOnPropertyChanged(
		std::function<void(uint32_t, const std::string&, const std::string&)> callback);
	void setOnOpenCodeEditor(OpenCodeEditorFn callback);
	void setCodeLightEditDraw(CodeLightEditDrawFn callback);
	/** @brief Invoke the Open-in-Code-Editor hook when one is registered. */
	void openInCodeEditor(uint32_t entity);

	/// Pack-owned inspectors (e.g. selected node graph params).
	void addExtraDrawer(ExtraDrawer drawer);

  private:
	uint32_t m_selectedEntity = 0;
	float m_entityListHeight = 100.0f;
	bool m_entityListOpen = true;
	bool m_entityListStateLoaded = false;
	float m_codeEditHeight = 220.0f;
	bool m_codeEditHeightLoaded = false;
	std::vector<ExtraDrawer> m_extraDrawers;

	std::function<void(uint32_t)> m_onEntitySelected;
	std::function<void(uint32_t, const std::string&, const std::string&)> m_onPropertyChanged;
	OpenCodeEditorFn m_onOpenCodeEditor;
	CodeLightEditDrawFn m_codeLightEditDraw;

	void renderContents() override;
	void renderEntityList();
	void renderEntityListGrip();
	void renderCodeEditHeightGrip();
	/// Entity list height + open state live in the settings blob, not imgui.ini.
	void loadEntityListState();
	void saveEntityListState();
	void loadCodeEditHeight();
	void saveCodeEditHeight();
	MSettings* settings() const;
	void renderAllComponentProperties();
	/** @brief Collapsible Preview / Edit for entities with `CCode`. */
	void renderCodeEditSection(MEcs& ecs, entt::entity entity);
};

} // namespace rigkit
