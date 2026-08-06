#pragma once

#include <string>

namespace rigkit {

/**
 * @brief Read the default framebuffer and write a PNG next to data/export/.
 * @return Absolute path written, or empty on failure.
 */
std::string exportFramebufferPng(int width, int height, const std::string& suggestedName = "export");

} // namespace rigkit
