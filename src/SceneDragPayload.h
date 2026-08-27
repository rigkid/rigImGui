#pragma once

#include <cstdint>

namespace rigkit {

/// Scene hierarchy drag (`SceneWindow` to reparent / Node Editor entity ref).
inline constexpr const char* kRigSceneEntityPayload = "RIG_SCENE_ENTITY";

/// Field patch pin to Node Editor bound `ref.*` (`offerScenePropDrag` / `RenderProps`).
inline constexpr const char* kRigScenePropPayload = "RIG_SCENE_PROP";

/**
 * @brief Payload for `kRigScenePropPayload`.
 * @details `propType` is a `propTypes` value (`EPT_FLOAT`, ...). `name` must match
 * `GetProperties()` / drive-slot names so `applyRefWrites` can write the field.
 *
 * Any RigKit app: register `rigNodeEditor`. Properties (`RenderProps(..., entityId)`)
 * emit pins automatically. Custom ImGui: `PropDragSource src(entityId);` widget;
 * `offerScenePropDrag("Width", EPT_INT);` then drag the field name (or Alt-drag
 * the value) onto the Node Editor. Alt+drop adds an LFO on floats.
 */
struct RigScenePropPayload {
	uint32_t entity = 0;
	char name[96]{};
	int propType = 0;
};

} // namespace rigkit
