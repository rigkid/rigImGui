#include "FontPicker.h"

#include "core/IMui.h"
#include "core/util/AppPaths.h"
#include "rendering/U_gladGlfw.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace rigkit {
namespace FontPicker {
namespace {

bool isFontExt(const std::filesystem::path& p) {
	auto e = p.extension().string();
	for (char& c : e) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return e == ".ttf" || e == ".otf" || e == ".ttc";
}

} // namespace

std::vector<std::string> listInstalledFonts() {
	namespace fs = std::filesystem;
	std::vector<std::string> out;
	const fs::path dir = AppPaths::getFontsDir();
	std::error_code ec;
	if (!fs::is_directory(dir, ec)) {
		return out;
	}
	for (const auto& ent : fs::directory_iterator(dir, ec)) {
		if (!ent.is_regular_file(ec)) {
			continue;
		}
		if (!isFontExt(ent.path())) {
			continue;
		}
		out.push_back(ent.path().filename().string());
	}
	std::sort(out.begin(), out.end());
	return out;
}

std::string resolveFontPath(const std::string& pathOrName) {
	namespace fs = std::filesystem;
	if (pathOrName.empty()) {
		return {};
	}
	fs::path p(pathOrName);
	std::error_code ec;
	if (p.is_absolute() && fs::is_regular_file(p, ec)) {
		return p.lexically_normal().string();
	}
	const fs::path underFonts = fs::path(AppPaths::getFontsDir()) / p;
	if (fs::is_regular_file(underFonts, ec)) {
		return underFonts.lexically_normal().string();
	}
	if (fs::is_regular_file(p, ec)) {
		return fs::absolute(p, ec).lexically_normal().string();
	}
	return {};
}

bool draw(const char* strId, std::string& path, float* sizePx, IMui* ui,
		  std::function<void(std::string)> onBrowse) {
	bool changed = false;
	ImGui::PushID(strId);

	const auto fonts = listInstalledFonts();
	int selected = -1;
	for (int i = 0; i < static_cast<int>(fonts.size()); ++i) {
		if (fonts[static_cast<size_t>(i)] == path ||
			resolveFontPath(fonts[static_cast<size_t>(i)]) == resolveFontPath(path)) {
			selected = i;
			break;
		}
	}

	const char* preview = path.empty() ? "(default)" : path.c_str();
	if (ImGui::BeginCombo("Font", preview)) {
		if (ImGui::Selectable("(default)", path.empty())) {
			if (!path.empty()) {
				path.clear();
				changed = true;
			}
			selected = -1;
		}
		for (int i = 0; i < static_cast<int>(fonts.size()); ++i) {
			const bool isSel = (i == selected);
			if (ImGui::Selectable(fonts[static_cast<size_t>(i)].c_str(), isSel)) {
				if (path != fonts[static_cast<size_t>(i)]) {
					path = fonts[static_cast<size_t>(i)];
					changed = true;
				}
			}
			if (isSel) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ui) {
		if (ImGui::Button("Browse...")) {
			ui->openFileDialog("Choose Font", {".ttf", ".otf", ".ttc"},
							   [onBrowse, &path](const std::string& picked) {
								   if (onBrowse) {
									   onBrowse(picked);
								   } else {
									   path = picked;
								   }
							   });
		}
	} else {
		ImGui::BeginDisabled();
		ImGui::Button("Browse...");
		ImGui::EndDisabled();
	}

	if (sizePx) {
		if (ImGui::DragFloat("Font size", sizePx, 0.5f, 8.f, 128.f, "%.0f")) {
			*sizePx = std::clamp(*sizePx, 8.f, 128.f);
			changed = true;
		}
	}

	ImGui::TextDisabled("Fonts folder: %s", AppPaths::getFontsDir().c_str());
	ImGui::PopID();
	return changed;
}

bool bakeGlyphAtlas(const std::string& fontPathOrName, float sizePixels, const char* codepoints,
					unsigned& outTexture, int& outW, int& outH, std::vector<float>& outUVs) {
	outTexture = 0;
	outW = outH = 0;
	outUVs.clear();
	if (!codepoints || !*codepoints) {
		return false;
	}

	float size = sizePixels;
	if (size < 8.f) {
		size = 8.f;
	}
	if (size > 128.f) {
		size = 128.f;
	}

	ImFontAtlas atlas;
	ImFontConfig cfg;
	cfg.FontDataOwnedByAtlas = true;
	static const ImWchar kRanges[] = {0x0020, 0x00FF, 0};
	ImFont* font = nullptr;
	const std::string resolved = resolveFontPath(fontPathOrName);
	if (!resolved.empty()) {
		font = atlas.AddFontFromFileTTF(resolved.c_str(), size, &cfg, kRanges);
	}
	if (!font) {
		font = atlas.AddFontDefault(&cfg);
	}
	if (!font) {
		spdlog::warn("[FontPicker] Failed to load font for atlas bake");
		return false;
	}

	unsigned char* pixels = nullptr;
	int w = 0, h = 0;
	atlas.GetTexDataAsRGBA32(&pixels, &w, &h);
	if (!pixels || w <= 0 || h <= 0) {
		spdlog::warn("[FontPicker] Font atlas bake produced no pixels");
		return false;
	}

	unsigned tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);

	ImFontBaked* baked = font->GetFontBaked(size);
	if (!baked) {
		destroyGlyphAtlasTexture(tex);
		return false;
	}

	const int n = static_cast<int>(std::strlen(codepoints));
	outUVs.resize(static_cast<size_t>(n) * 4);
	for (int i = 0; i < n; ++i) {
		const ImWchar cp = static_cast<ImWchar>(static_cast<unsigned char>(codepoints[i]));
		const ImFontGlyph* g = baked->FindGlyph(cp);
		if (!g) {
			outUVs[static_cast<size_t>(i) * 4 + 0] = 0.f;
			outUVs[static_cast<size_t>(i) * 4 + 1] = 0.f;
			outUVs[static_cast<size_t>(i) * 4 + 2] = 0.f;
			outUVs[static_cast<size_t>(i) * 4 + 3] = 0.f;
			continue;
		}
		outUVs[static_cast<size_t>(i) * 4 + 0] = g->U0;
		outUVs[static_cast<size_t>(i) * 4 + 1] = g->V0;
		outUVs[static_cast<size_t>(i) * 4 + 2] = g->U1;
		outUVs[static_cast<size_t>(i) * 4 + 3] = g->V1;
	}

	outTexture = tex;
	outW = w;
	outH = h;
	return true;
}

void destroyGlyphAtlasTexture(unsigned texture) {
	if (texture) {
		glDeleteTextures(1, &texture);
	}
}

} // namespace FontPicker
} // namespace rigkit
