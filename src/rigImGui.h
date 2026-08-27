#pragma once

#include "core/pack/IPack.h"
#include "Mui.h"

namespace rigkit {
class rigImGui : public rigkit::IPack {
  public:
	rigImGui();
	bool init() override;
	void setup() override;
	void update(float dt) override;
	void draw() override {}
	void cleanup() override;

  private:
	Mui *m_ui = nullptr;
};
} // namespace rigkit
