#pragma once

#include "IWindow.h"
#include "core/util/View2D.h"

namespace rigkit {

/**
 * @brief Secondary 2D viewport panel — pan/zoom over View2D + optional rulers.
 * @details Apps set content size and optionally a GL texture id to display.
 * Multi-FBO authoring can bind a Canvas texture here; default is empty grid.
 */
class ViewportWindow : public IWindow {
  public:
	explicit ViewportWindow(const std::string& title = "Viewport");

	View2D& view() { return m_view; }
	const View2D& view() const { return m_view; }

	void setTexture(unsigned int textureId, int texW, int texH);
	void clearTexture();

	bool rulersVisible() const { return m_rulers; }
	void setRulersVisible(bool v) { m_rulers = v; }

  protected:
	void renderContents() override;

  private:
	View2D m_view;
	unsigned int m_texture = 0;
	int m_texW = 0;
	int m_texH = 0;
	bool m_rulers = true;
	bool m_panning = false;
};

} // namespace rigkit
