#pragma once

/**
 * @file
 * @brief Dark/Light bases, color-scheme JSON, and UI font loading for rigImGui.
 */

#include <string>
#include <vector>
#include "core/json.h"

struct ImGuiIO;
struct ImGuiStyle;

namespace rigkit {

/// Built-in bases. Named palettes are JSON color schemes, not more enum values.
enum class ImGuiTheme { Dark = 0, Light };

inline constexpr int kImGuiThemeCount = 2;

inline ImGuiTheme clampImGuiTheme(int theme) {
	if (theme < 0 || theme >= kImGuiThemeCount) {
		return ImGuiTheme::Dark;
	}
	return static_cast<ImGuiTheme>(theme);
}

/// One JSON color scheme from shipped or user themes folders.
struct ThemeFile {
	std::string fileName;
	std::string path;
	std::string name;
	std::string credit;
	std::string source;
	std::string license;
	bool shipped = false;
};

/// Themes, fonts, and style extras for rigImGui.
namespace ImGuiStyleKit {

void applyTheme(ImGuiTheme theme);
void applyStyleExtras(ImGuiTheme theme);

/// Map leftover prefs ints onto Dark/Light and a scheme filename.
void migrateLegacyTheme(int& theme, std::string& themeFile);

std::vector<ThemeFile> listThemeFiles();
std::string resolveThemePath(const std::string& pathOrName);
std::string themePathForSave(const std::string& pathOrName);
const ThemeFile* findThemeFile(const std::vector<ThemeFile>& files, const std::string& pathOrName);

json styleToJson(const ImGuiStyle& style, int baseTheme = 0);
bool jsonToStyle(const json& j, ImGuiStyle& style, int* outBaseTheme = nullptr);

bool saveStyleToFile(const std::string& path, const ImGuiStyle& style, int baseTheme = 0);
bool loadStyleFromFile(const std::string& path, ImGuiStyle& style, int* outBaseTheme = nullptr);

bool loadFonts(ImGuiIO& io, const std::string& fontsSearchDir = {},
			   const std::string& bodyFontPath = {}, float sizePixels = 16.0f,
			   float weight = 400.f);

std::string resolveBodyFontPath(const std::string& fontsSearchDir = {},
								const std::string& bodyFontPath = {});

} // namespace ImGuiStyleKit
} // namespace rigkit
