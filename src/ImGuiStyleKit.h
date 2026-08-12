#pragma once

#include <string>
#include "core/json.h"

struct ImGuiIO;
struct ImGuiStyle;

namespace rigkit {

/// Built-in rigImGui themes.
enum class ImGuiTheme { Dark = 0, Light, Classic, Corporate, Dracula };

inline constexpr int kImGuiThemeCount = 5;

inline ImGuiTheme clampImGuiTheme(int theme) {
	if (theme < 0 || theme >= kImGuiThemeCount) {
		return ImGuiTheme::Dark;
	}
	return static_cast<ImGuiTheme>(theme);
}

/// Themes, fonts, and style extras for rigImGui.
/// Lives in rigImGui (not a separate pack) so default UI stays one pack.
namespace ImGuiStyleKit {

void applyTheme(ImGuiTheme theme);
void applyStyleExtras(ImGuiTheme theme);

/// Serialize current ImGui style (full ShowStyleEditor surface) to JSON.
/// Portable theme save/load workflow.
json styleToJson(const ImGuiStyle& style, int baseTheme = 0);
/// Apply JSON onto a style object (missing keys leave values unchanged).
bool jsonToStyle(const json& j, ImGuiStyle& style, int* outBaseTheme = nullptr);

/// Write / read style JSON files (creates parent dirs on save).
bool saveStyleToFile(const std::string& path, const ImGuiStyle& style, int baseTheme = 0);
bool loadStyleFromFile(const std::string& path, ImGuiStyle& style, int* outBaseTheme = nullptr);

/// Load UI fonts. Empty bodyFontPath → Roboto (or ImGui default). Merges Font Awesome when present.
/// @param bodyFontPath Absolute path, or filename relative to fontsSearchDir.
/// @param sizePixels Body font size (clamped to a sane range).
bool loadFonts(ImGuiIO& io, const std::string& fontsSearchDir = {},
			   const std::string& bodyFontPath = {}, float sizePixels = 16.0f);

/// @deprecated Prefer loadFonts()
inline void loadDefaultFonts(ImGuiIO& io, const std::string& fontsSearchDir = {}) {
	loadFonts(io, fontsSearchDir, {}, 16.0f);
}

} // namespace ImGuiStyleKit
} // namespace rigkit
