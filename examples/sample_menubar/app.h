#pragma once

#include "core/U_core.h"

class SampleMenubarApp : public rigkit::IApp {
  public:
	SampleMenubarApp();
	void setup() override;
	void update(float) override {}
	void draw() override {}
};
