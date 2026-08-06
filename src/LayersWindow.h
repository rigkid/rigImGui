#pragma once

#include "IWindow.h"

namespace rigkit {

/** @brief Lists entities with CLayer; reorder + select. */
class LayersWindow : public IWindow {
  public:
	explicit LayersWindow(const std::string& title = "Layers");

  protected:
	void renderContents() override;
};

} // namespace rigkit
