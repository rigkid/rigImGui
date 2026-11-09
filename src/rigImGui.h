#pragma once

#include "core/pack/IPack.h"
#include "Mui.h"

namespace rigkit {

class RigKitEngine;

class rigImGui : public IPack {
  public:
	rigImGui();
	bool init() override;
	void setup() override;
	void update(float dt) override;
	void draw() override {}
	void cleanup() override;
	void onUiAttached() override;

	/**
	 * @brief ImGui fulfillment when chrome is imgui; otherwise null.
	 * @details Author apps hold this pointer (rebind after chrome swap).
	 */
	Mui* mui() const { return m_ui; }

  private:
	Mui* m_ui = nullptr;
};

/**
 * @brief Author ImGui chrome for @p engine, or null when idle / not imgui.
 * @details Prefer holding the result on the app; re-read in `onUiAttached`.
 */
Mui* authorMui(RigKitEngine& engine);

} // namespace rigkit
