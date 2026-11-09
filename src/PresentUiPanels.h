#pragma once

/**
 * @file
 * @brief Draw visible @c rig\.ui\.* panels from ECS data.
 */

namespace rigkit {

class MEcs;

/**
 * @brief Present every visible `CUiPanel` as a tool window or HUD overlay.
 * @details Controls bind `target` + `propertyKey` through `GetProperties`.
 * Actions fire `SEvent` ids registered in `IApp::registerUIActions`. Unknown
 * action ids stay hidden. Does not create `ImGuiPanel` entries.
 */
void presentUiPanels(MEcs& ecs);

} // namespace rigkit
