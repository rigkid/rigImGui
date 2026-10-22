#pragma once

#include <cstdint>

namespace rigkit {

/// Scene hierarchy drag (`SceneWindow` to reparent / Node Editor entity ref).
inline constexpr const char* kRigSceneEntityPayload = "RIG_SCENE_ENTITY";

/// Field patch pin to Node Editor / Patch Destination (`offerScenePropDrag` / `RenderProps`).
inline constexpr const char* kRigScenePropPayload = "RIG_SCENE_PROP";

/// Existing graph node dropped onto a Patch Destination combo.
inline constexpr const char* kRigGraphNodePayload = "RIG_GRAPH_NODE";

/**
 * @brief Payload for `kRigScenePropPayload`.
 * @details `propType` is a `propTypes` value (`EPT_FLOAT`, ...). `name` must match
 * `GetProperties()` / drive-slot names so `applyRefWrites` can write the field.
 *
 * Properties (`RenderProps(..., entityId)`) emit pins automatically. Custom
 * ImGui: `PropDragSource src(entityId);` widget;
 * `offerScenePropDrag("Width", EPT_INT);` then drag the field name (or Alt-drag
 * the value) onto the Node Editor or a Patch Destination. Alt+drop adds an LFO
 * on floats.
 */
struct RigScenePropPayload {
	uint32_t entity = 0;
	char name[96]{};
	int propType = 0;
	/// Owning ECS (opaque). Null = engine registry. Multi-scene hosts tag
	/// drags so an editor bound to another ECS rejects the drop - entt ids
	/// are only unique within one registry.
	const void* ecsTag = nullptr;
};

/**
 * @brief Payload for `kRigGraphNodePayload`.
 * @details `pin` is an input pin id, or 0 to pick the first compatible In.
 */
struct RigGraphNodePayload {
	uint32_t node = 0;
	uint32_t pin = 0;
	const void* ecsTag = nullptr;
};

} // namespace rigkit
