#pragma once

#include <string>

namespace rigkit {
class MEcs;
}
#include <imgui.h>
#include "IWindow.h"

namespace rigkit {

class DebugPanel : public IWindow {
  public:
	DebugPanel(const std::string &title = "Debug Panel###DebugPanel",
			   ImGuiWindowFlags flags = 0);
	virtual ~DebugPanel() = default;

	// Debug functionality
	void setDeltaTime(float deltaTime) { m_deltaTime = deltaTime; }

	void renderContents() override;

  private:
	enum class ProgressDemo { None, StepBased, Absolute, Indeterminate };

	float m_deltaTime = 0.0f;

	ProgressDemo m_progressDemo = ProgressDemo::None;
	int m_stepCurrent = 0;
	int m_stepTotal = 80;
	float m_absolutePos = 0.f;
	float m_stepAccumulator = 0.f;
	float m_simulationSpeed = 1.0f;

	void renderDebugInfo();
	void renderClearShapesButton();
	void renderEntityInfo();
	void renderPerformanceInfo();
	void renderProgressDemo();
	void tickProgressDemo(float dt);
};

} // namespace rigkit
