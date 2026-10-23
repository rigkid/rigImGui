#pragma once

#include "ecs/PropertyReflection.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace rigkit {

/**
 * @brief Value snapshot of one `sProp` field for undo records.
 * @details Int-backed types (int / uint / enum) share the int alternative,
 * matching how the editors write them. Vec4 covers color too.
 */
using PropValue =
	std::variant<bool, int, float, double, std::string, glm::vec2, glm::vec3, glm::vec4>;

/** @brief One committed widget edit: drag released, checkbox toggled, text defocused. */
struct PropCommit {
	uint32_t propId = 0;
	std::string propName;
	PropValue before;
	PropValue after;
};

/** @brief Called by RenderProps when an edit commits; hosts push undo records here. */
using PropCommitFn = std::function<void(const PropCommit&)>;

/**
 * @brief "No entity" sentinel for the prop-drag API (`entt::to_integral(entt::null)`).
 * @details Entity id 0 is a real entt entity (the first one created), so 0
 * must NOT be used to mean "none".
 */
inline constexpr uint32_t kPropDragNoEntity = 0xFFFFFFFFu;

/** @brief Read the field behind @p prop into a PropValue (unsupported types read as int 0). */
PropValue readPropValue(const sProp& prop);

/** @brief Write @p value back through @p prop.data when the type alternative matches. */
void writePropValue(const sProp& prop, const PropValue& value);

/**
 * @brief Edit `sProp` fields with ImGui. Returns true if any value changed.
 * @param headerName CollapsingHeader label; null = draw widgets only.
 * @param entityId When not `kPropDragNoEntity`, each row gets a patch pin
 * (`RIG_SCENE_PROP`) for the Node Editor - the automatic path for any app using
 * Properties.
 * @param onCommit When set, fires once per finished edit (a whole drag = one commit)
 * with before/after values - the seam for undo records.
 */
bool RenderProps(const char* headerName, std::vector<sProp>& props,
				 uint32_t entityId = kPropDragNoEntity, const PropCommitFn& onCommit = {});

/**
 * @brief Default entity for `offerScenePropDrag(name, type)` (nested; last Begin wins).
 * @details Custom panels: `PropDragSource src(entityId);` then widget +
 * `offerScenePropDrag("Width", EPT_INT);`. Properties / `RenderProps` do this
 * for you.
 */
void BeginPropDragSource(uint32_t entityId);
void EndPropDragSource();
uint32_t currentPropDragEntity();

/**
 * @brief Make the last widget's field name a Node Editor drop source.
 * @details Overlays the label (right of the value). Drag the name; the number
 * still edits. Hold Alt to drag from the value itself. `propName` is the
 * `GetProperties()` / drive-slot name `applyRefWrites` matches.
 * `entityId == kPropDragNoEntity` uses `BeginPropDragSource`.
 */
/// @param ecsTag Owning ECS (opaque) for multi-scene hosts - see
/// `RigScenePropPayload::ecsTag`. Null = engine registry.
/// @param hasRange When true, Sound patches seed the Map output with
/// @p rangeMin / @p rangeMax (the slider limits).
void offerScenePropDrag(uint32_t entityId, const char* propName, int propType,
						const void* ecsTag = nullptr, float rangeMin = 0.f, float rangeMax = 1.f,
						bool hasRange = false);
void offerScenePropDrag(const char* propName, int propType);

/**
 * @brief Queued "patch this prop into the Node Editor" request.
 * @details Emitted by the right-click menu on prop drag pins (`offerScenePropDrag`)
 * as a zero-drag alternative to drag-drop. The Node Editor drains the queue once
 * per frame via `takeScenePropPatchRequests` and spawns the matching `ref.*` node
 * (plus a wired LFO when @ref withLfo is set, matching Alt+drop, or the catalog
 * node in @ref sourceType / @ref sourcePin. Sound uses `mod.audio`).
 * An LFO goes through a Map: input is the LFO span, output is the slider range.
 */
struct ScenePropPatchRequest {
	uint32_t entity = kPropDragNoEntity;
	std::string name;
	int propType = 0;
	bool withLfo = false;
	/// Catalog node wired into the ref (`mod.audio`, ...). Empty keeps @ref withLfo.
	std::string sourceType;
	/// Output pin on @ref sourceType (`level`, `bass`, ...). Empty means `out`.
	std::string sourcePin;
	/// When set, the Map in front of the ref uses this output range (the knob's
	/// slider limits). Sound leaves the Map input at 0..1. An LFO patch sets the
	/// input from that LFO's span (offset plus or minus amplitude).
	bool hasMapRange = false;
	float mapOutMin = 0.f;
	float mapOutMax = 1.f;
	/// Remove the ref driving this property (and a Map / LFO that only fed it).
	bool unpatch = false;
	/// Owning ECS (opaque, see `RigScenePropPayload::ecsTag`). Routes the
	/// request to the editor bound to that ECS; null = any editor.
	const void* ecsTag = nullptr;
	/// Existing `mod.lfo` to wire instead of spawning one. 0 spawns a new LFO.
	/// Ignored unless @ref withLfo is set. The id belongs to @ref ecsTag's graph.
	uint32_t reuseLfo = 0;
};

/**
 * @brief One row in the property right-click menu (Sound → Level, and so on).
 * @details Filled from the node catalog so new audio outputs show up without
 * a menu edit. Empty list falls back to the built-in `mod.audio` pins.
 */
struct ScenePropPatchSource {
	std::string group;
	std::string typeId;
	std::string pin;
	std::string label;
};

/** @brief Replace the right-click source list (Sound, and any later groups). */
void setScenePropPatchSources(std::vector<ScenePropPatchSource> sources);

/**
 * @brief One existing LFO the property menu can wire instead of spawning.
 * @details The Node Editor publishes the active graph's `mod.lfo` nodes, keyed
 * by the same ECS pointer as `RigScenePropPayload::ecsTag`. @ref nodeId is only
 * meaningful in that graph.
 */
struct ScenePropLfoChoice {
	uint32_t nodeId = 0;
	std::string label;
};

/**
 * @brief Replace the existing-LFO list for @p ecsTag.
 * @details Pass an empty list when the graph has no LFOs. Other ECS entries
 * stay. The property menu reads this while it is open.
 */
void setScenePropLfoChoices(const void* ecsTag, std::vector<ScenePropLfoChoice> choices);

/** @brief Queue a patch request (no-op when @p entityId is `kPropDragNoEntity`). */
void requestScenePropPatch(uint32_t entityId, const char* propName, int propType, bool withLfo,
						   const void* ecsTag = nullptr, const char* sourceType = nullptr,
						   const char* sourcePin = nullptr, bool hasMapRange = false,
						   float mapOutMin = 0.f, float mapOutMax = 1.f, bool unpatch = false,
						   uint32_t reuseLfo = 0);

/**
 * @brief Drain pending patch requests for the editor bound to @p consumerEcs.
 * @details Takes untagged requests plus those tagged with @p consumerEcs; requests
 * tagged for another ECS stay queued for that editor (multi-deck hosts render one
 * editor per deck). Requests older than a couple frames are dropped either way.
 * Null @p consumerEcs takes everything (single-editor apps).
 */
std::vector<ScenePropPatchRequest> takeScenePropPatchRequests(const void* consumerEcs = nullptr);

/** @brief RAII `BeginPropDragSource` / `EndPropDragSource`. */
struct PropDragSource {
	explicit PropDragSource(uint32_t entityId) { BeginPropDragSource(entityId); }
	~PropDragSource() { EndPropDragSource(); }
	PropDragSource(const PropDragSource&) = delete;
	PropDragSource& operator=(const PropDragSource&) = delete;
};

} // namespace rigkit
