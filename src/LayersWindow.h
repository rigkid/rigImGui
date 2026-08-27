#pragma once

#include "IWindow.h"

#include <string>

namespace rigkit {

/** @brief Lists entities with CLayer; reorder + select. */
class LayersWindow : public IWindow {
  public:
	explicit LayersWindow(const std::string& title = "Layers");

  protected:
	void renderContents() override;
};

} // namespace rigkit
