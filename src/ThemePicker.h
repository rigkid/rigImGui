#pragma once

/**
 * @file
 * @brief Color-scheme combo over shipped + user theme JSON.
 */

#include <string>

namespace rigkit {

namespace ThemePicker {

/// Combo of catalog JSON + None. Writes a relative filename (or empty).
/// @return true if the selection changed.
bool draw(const char* strId, std::string& themeFile);

} // namespace ThemePicker
} // namespace rigkit
