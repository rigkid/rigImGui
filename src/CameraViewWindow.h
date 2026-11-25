#pragma once

#include <string>
#include "COrbitDrive.h"
#include "ImGuiPanel.h"
#include "OrbitNav.h"
#include "SMeshPresent3D.h"

namespace rigkit {
class Mui;

/**
 * @brief Free-orbit 3D camera panel — one `COrbitDrive` camera to an FBO.
 * @details Present via `MeshPresentTarget` + `addImagePresent` (BottomLeft).
 * Mouse truck / tumble / zoom only through `rig::orbitNavigate` — do not invent
 * panel-local signs. Host Space cameras set `COrbitDrive::worldUp` to Z.
 * Locked ortho faces stay on `OrthoViewWindow`.
 */
class CameraViewWindow : public ImGuiPanel {
  public:
	enum class Preset : int { Top = 0, Front, Back, Left, Right, Iso };

	CameraViewWindow(const std::string& title, std::string cameraName);

	const std::string& cameraName() const { return m_cameraName; }

	/** @brief Aim + frame from scene AABB (or keep current target span). */
	void applyPreset(Preset preset);

  protected:
	void renderContents() override;

  private:
	std::string m_cameraName;
	ecs::MeshPresentTarget m_target;
	rig::OrbitNavState m_nav;
	bool m_havePivotPress = false;
};

/**
 * @brief Create an inactive free-orbit camera and a dockable window.
 * @param worldUp Host Space (+Z) or CAD (+Y).
 */
entt::entity openCameraView(Mui& ui,
							ecs::COrbitDrive::WorldUp worldUp = ecs::COrbitDrive::WorldUp::Y);

/** @brief View → Camera → Free Orbit (new instance each pick). */
void registerCameraViewMenu(Mui& ui);

} // namespace rigkit
