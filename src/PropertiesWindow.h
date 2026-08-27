#pragma once

#include <functional>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

#include "ecs/MEcs.h"
#include "IWindow.h"

namespace rigkit {

class MSettings;

class PropertiesWindow : public IWindow {
  public:
	using ExtraDrawer = std::function<void(MEcs& ecs, entt::entity entity)>;
	/** @brief Optional - when set, the inline editor shows "Open in Code Editor". */
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

	/// Empty inspector. Not `0` - EnTT's first entity is often id 0.
	static constexpr uint32_t kNoEntity = static_cast<uint32_t>(-1);

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

	/**
	 * @brief Called after a component header's stock props, while that header is open.
	 * @details Use this to replace a string field with a catalog combo (PlotLayer tool).
	 */
	using AfterComponentFn =
		std::function<void(const std::string& component, MEcs& ecs, entt::entity entity)>;
	void setAfterComponentDraw(AfterComponentFn fn);

  private:
	uint32_t m_selectedEntity = kNoEntity;
	float m_entityListHeight = 100.0f;
	bool m_entityListOpen = true;
	bool m_entityListStateLoaded = false;
	float m_codeEditHeight = 220.0f;
	bool m_codeEditHeightLoaded = false;
	std::vector<ExtraDrawer> m_extraDrawers;
	AfterComponentFn m_afterComponent;

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
	/** @brief Inline code editor for entities with `CCode`. */
	void renderCodeEditSection(MEcs& ecs, entt::entity entity);
};

} // namespace rigkit
