#pragma once

#include <functional>
#include <string>
#include <vector>

namespace rigkit {
class IMui;

/**
 * @brief Reusable font path + optional size control for Preferences / effect UIs.
 * @details Lists TTFs under AppPaths::getFontsDir(), Browse via IMui file dialog.
 */
namespace FontPicker {

/** @brief Filenames (not full paths) found under the fonts data directory. */
std::vector<std::string> listInstalledFonts();

/** @brief Resolve relative name against fonts dir, or return absolute path as-is. */
std::string resolveFontPath(const std::string& pathOrName);

/**
 * @brief Draw combo of installed fonts + Browse… (+ optional size).
 * @param sizePx Null to hide size control.
 * @param ui Used for Browse (openFileDialog). May be null (Browse disabled).
 * @param onBrowse Optional; when set, Browse uses this instead of writing `path`
 *        (required when `path` is a temporary).
 * @return true if path or size changed this frame (sync edits only).
 */
bool draw(const char* strId, std::string& path, float* sizePx, IMui* ui,
		  std::function<void(std::string)> onBrowse = nullptr);

/**
 * @brief Bake a standalone glyph atlas (ImGui atlas builder → GL texture).
 * @details Does not touch the live UI font. Caller owns `outTexture` (glDeleteTextures).
 * `outUVs` is 4 floats per character in `codepoints` (u0,v0,u1,v1).
 */
bool bakeGlyphAtlas(const std::string& fontPathOrName, float sizePixels, const char* codepoints,
					unsigned& outTexture, int& outW, int& outH, std::vector<float>& outUVs);

void destroyGlyphAtlasTexture(unsigned texture);

} // namespace FontPicker
} // namespace rigkit
