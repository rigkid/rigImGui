#pragma once

#include "core/U_core.h"

class ImGuizmoApp : public rigkit::IApp {
  public:
	ImGuizmoApp();
	void setup() override;
	void update(float) override {}
	void draw() override {}

  private:
	float m_matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};
