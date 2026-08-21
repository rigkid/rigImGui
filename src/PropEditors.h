#pragma once

#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <string>
#include <variant>
#include <vector>
#include "ecs/PropertyReflection.h"

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

/** @brief Read the field behind @p prop into a PropValue (unsupported types read as int 0). */
PropValue readPropValue(const sProp& prop);

/** @brief Write @p value back through @p prop.data when the type alternative matches. */
void writePropValue(const sProp& prop, const PropValue& value);

/**
 * @brief Edit `sProp` fields with ImGui. Returns true if any value changed.
 * @param headerName CollapsingHeader label; null = draw widgets only.
 * @param entityId When non-zero, each row gets a patch pin (`RIG_SCENE_PROP`)
 * for the Node Editor — the automatic path for any app using Properties.
 * @param onCommit When set, fires once per finished edit (a whole drag = one commit)
 * with before/after values — the seam for undo records.
 */
bool RenderProps(const char* headerName, std::vector<sProp>& props, uint32_t entityId = 0,
				 const PropCommitFn& onCommit = {});

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
 * `entityId == 0` uses `BeginPropDragSource`.
 */
void offerScenePropDrag(uint32_t entityId, const char* propName, int propType);
void offerScenePropDrag(const char* propName, int propType);

/** @brief RAII `BeginPropDragSource` / `EndPropDragSource`. */
struct PropDragSource {
	explicit PropDragSource(uint32_t entityId) { BeginPropDragSource(entityId); }
	~PropDragSource() { EndPropDragSource(); }
	PropDragSource(const PropDragSource&) = delete;
	PropDragSource& operator=(const PropDragSource&) = delete;
};

} // namespace rigkit
