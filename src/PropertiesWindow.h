#pragma once

#include <functional>
#include <imgui.h>
#include <memory>
#include <vector>
#include "IWindow.h"
#include "ecs/MEcs.h"

namespace rigkit {

class MSettings;

class PropertiesWindow : public IWindow {
  public:
	using ExtraDrawer = std::function<void(MEcs& ecs, entt::entity entity)>;

	PropertiesWindow(const std::string& title = "Properties", ImGuiWindowFlags flags = 0);
	virtual ~PropertiesWindow() = default;

	void setSelectedEntity(uint32_t entity);
	uint32_t getSelectedEntity() const;

	void setOnEntitySelected(std::function<void(uint32_t)> callback);
	void setOnPropertyChanged(
		std::function<void(uint32_t, const std::string&, const std::string&)> callback);

	/// Pack-owned inspectors (e.g. selected node graph params).
	void addExtraDrawer(ExtraDrawer drawer);

  private:
	uint32_t m_selectedEntity = 0;
	float m_entityListHeight = 100.0f;
	bool m_entityListOpen = true;
	bool m_entityListStateLoaded = false;
	std::vector<ExtraDrawer> m_extraDrawers;

	std::function<void(uint32_t)> m_onEntitySelected;
	std::function<void(uint32_t, const std::string&, const std::string&)> m_onPropertyChanged;

	void renderContents() override;
	void renderEntityList();
	void renderEntityListGrip();
	/// Entity list height + open state live in the settings blob, not imgui.ini.
	void loadEntityListState();
	void saveEntityListState();
	MSettings* settings() const;
	void renderAllComponentProperties();
};

} // namespace rigkit
