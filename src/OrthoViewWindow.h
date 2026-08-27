#pragma once

#include <string>
#include "COrthoView.h"
#include "IWindow.h"
#include "SMeshPresent3D.h"

namespace rigkit {
class Mui;

/**
 * @brief Instantiable ortho camera panel — one `COrthoView` entity to an FBO.
 * @details Apps register the View menu with `registerOrthoViewMenu`, or spawn a
 * window with `openOrthoView` / `createWindow<OrthoViewWindow>`. Needs
 * **rigRender3D** at link time. Pan (MMB / Alt+LMB) and wheel zoom write
 * `COrthoView::target` / `CCamera::orthoHeight`.
 */
class OrthoViewWindow : public IWindow {
  public:
	OrthoViewWindow(const std::string& title, std::string cameraName);

	const std::string& cameraName() const { return m_cameraName; }

  protected:
	void renderContents() override;

  private:
	std::string m_cameraName;
	ecs::MeshPresentTarget m_target;
	bool m_panning = false;
};

/**
 * @brief Create an inactive ortho camera and a dockable window.
 * @return The new camera entity, or `entt::null`.
 */
entt::entity openOrthoView(Mui& ui, ecs::COrthoView::Face face);

/** @brief Add View → Camera → Top / Bottom / Left / Right (new instance each pick). */
void registerOrthoViewMenu(Mui& ui);

} // namespace rigkit
