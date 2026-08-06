#pragma once

#include <cstdint>

namespace rigkit {

/// Scene hierarchy drag (`SceneWindow` → reparent / Node Editor entity ref).
inline constexpr const char* kRigSceneEntityPayload = "RIG_SCENE_ENTITY";

/// Properties row drag → Node Editor bound `ref.*` node.
inline constexpr const char* kRigScenePropPayload = "RIG_SCENE_PROP";

/**
 * @brief Payload for `kRigScenePropPayload`.
 * @details `propType` is a `propTypes` value (`EPT_FLOAT`, …).
 */
struct RigScenePropPayload {
	uint32_t entity = 0;
	char name[96]{};
	int propType = 0;
};

} // namespace rigkit
