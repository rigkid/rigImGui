#include "ExportPng.h"

#include "core/util/AppPaths.h"
#include "rendering/U_gladGlfw.h"

#include <algorithm>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

// Keep symbols private - other packs may already compile stb_image_write.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace rigkit {

std::string exportFramebufferPng(int width, int height, const std::string& suggestedName) {
	if (width <= 0 || height <= 0) {
		return {};
	}

	std::vector<unsigned char> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

	// OpenGL origin is bottom-left; flip rows for PNG.
	std::vector<unsigned char> flipped(rgba.size());
	const size_t rowBytes = static_cast<size_t>(width) * 4u;
	for (int y = 0; y < height; ++y) {
		const unsigned char* src = rgba.data() + static_cast<size_t>(height - 1 - y) * rowBytes;
		unsigned char* dst = flipped.data() + static_cast<size_t>(y) * rowBytes;
		std::copy(src, src + rowBytes, dst);
	}

	namespace fs = std::filesystem;
	const fs::path dir = fs::path(AppPaths::getDataDir()) / "export";
	std::error_code ec;
	fs::create_directories(dir, ec);
	const std::string base = suggestedName.empty() ? "export" : suggestedName;
	fs::path path = dir / (base + ".png");
	int n = 1;
	while (fs::exists(path)) {
		path = dir / (base + "_" + std::to_string(n++) + ".png");
	}

	const std::string out = path.lexically_normal().string();
	if (!stbi_write_png(out.c_str(), width, height, 4, flipped.data(),
						static_cast<int>(rowBytes))) {
		spdlog::error("[rigImGui] Failed to write PNG: {}", out);
		return {};
	}
	spdlog::info("[rigImGui] Exported PNG: {}", out);
	return out;
}

} // namespace rigkit
