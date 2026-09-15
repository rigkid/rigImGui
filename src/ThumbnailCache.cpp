#include "ThumbnailCache.h"

#include "rendering/U_gladGlfw.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_ONLY_TGA
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace rigkit {
namespace {

constexpr int kMaxEdge = 128;
constexpr int kMaxEntries = 128;
constexpr int kUploadsPerTick = 3;

} // namespace

ThumbnailCache::ThumbnailCache() = default;

ThumbnailCache::~ThumbnailCache() {
	stopWorker();
	// After Mui GL teardown, deleting textures is unsafe. shutdown() clears
	// them first; if that was skipped, drop the ids.
	m_entries.clear();
	m_lru.clear();
}

std::string ThumbnailCache::normalizeExt(const std::filesystem::path& path) {
	std::string ext = path.extension().string();
	for (char& c : ext) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return ext;
}

std::string ThumbnailCache::pathKey(const std::filesystem::path& path) {
	const std::u8string u8 = path.lexically_normal().u8string();
	return std::string(u8.begin(), u8.end());
}

std::string ThumbnailCache::pathUtf8(const std::filesystem::path& path) {
	const std::u8string u8 = path.u8string();
	return std::string(u8.begin(), u8.end());
}

void ThumbnailCache::registerDecoder(const std::string& extension, IMui::FileThumbnailDecoder decoder) {
	std::string ext = extension;
	if (!ext.empty() && ext.front() != '.') {
		ext.insert(ext.begin(), '.');
	}
	for (char& c : ext) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!decoder) {
		m_decoders.erase(ext);
		return;
	}
	m_decoders[ext] = std::move(decoder);
}

bool ThumbnailCache::canDecode(const std::filesystem::path& path) const {
	const std::string ext = normalizeExt(path);
	if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" ||
		ext == ".tga") {
		return true;
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_decoders.find(ext) != m_decoders.end();
}

IMui::FileThumbnailDecoder ThumbnailCache::decoderFor(const std::string& ext) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_decoders.find(ext);
	if (it == m_decoders.end()) {
		return {};
	}
	return it->second;
}

void ThumbnailCache::boxDownsample(const unsigned char* src, int sw, int sh, unsigned char* dst,
								   int dw, int dh) {
	for (int y = 0; y < dh; ++y) {
		const int sy = y * sh / dh;
		for (int x = 0; x < dw; ++x) {
			const int sx = x * sw / dw;
			const unsigned char* p =
				src +
				(static_cast<size_t>(sy) * static_cast<size_t>(sw) + static_cast<size_t>(sx)) * 4;
			unsigned char* q =
				dst +
				(static_cast<size_t>(y) * static_cast<size_t>(dw) + static_cast<size_t>(x)) * 4;
			std::memcpy(q, p, 4);
		}
	}
}

bool ThumbnailCache::decodeFile(const std::filesystem::path& path, Raster& out) {
	if (auto decoder = decoderFor(normalizeExt(path))) {
		IMui::FileThumbnailPixels pix;
		if (!decoder(pathUtf8(path), kMaxEdge, pix) || pix.w < 1 || pix.h < 1 ||
			pix.rgba.size() < static_cast<size_t>(pix.w) * static_cast<size_t>(pix.h) * 4) {
			return false;
		}
		out.w = pix.w;
		out.h = pix.h;
		out.rgba = std::move(pix.rgba);
		return true;
	}

	// Raster formats shipped with the dialogs (stb). Packs add others.
	// ifstream + path uses the native wide API on Windows; stbi_load(char*) does not.
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return false;
	}
	in.seekg(0, std::ios::end);
	const std::streamoff len = in.tellg();
	if (len <= 0 || len > 16 * 1024 * 1024) {
		return false;
	}
	in.seekg(0, std::ios::beg);
	std::vector<unsigned char> file(static_cast<size_t>(len));
	if (!in.read(reinterpret_cast<char*>(file.data()), len)) {
		return false;
	}

	int infoW = 0;
	int infoH = 0;
	int infoComp = 0;
	if (!stbi_info_from_memory(file.data(), static_cast<int>(file.size()), &infoW, &infoH,
							   &infoComp) ||
		infoW < 1 || infoH < 1 || std::max(infoW, infoH) > 8192) {
		return false;
	}

	int w = 0;
	int h = 0;
	int comp = 0;
	unsigned char* pixels = stbi_load_from_memory(file.data(), static_cast<int>(file.size()), &w, &h,
												  &comp, 4);
	if (!pixels || w < 1 || h < 1) {
		if (pixels) {
			stbi_image_free(pixels);
		}
		return false;
	}

	int uw = w;
	int uh = h;
	const unsigned char* src = pixels;
	std::vector<std::uint8_t> shrink;
	const int edge = std::max(w, h);
	if (edge > kMaxEdge) {
		uw = std::max(1, w * kMaxEdge / edge);
		uh = std::max(1, h * kMaxEdge / edge);
		shrink.resize(static_cast<size_t>(uw) * static_cast<size_t>(uh) * 4);
		boxDownsample(pixels, w, h, shrink.data(), uw, uh);
		src = shrink.data();
	}

	out.w = uw;
	out.h = uh;
	out.rgba.assign(src, src + static_cast<size_t>(uw) * static_cast<size_t>(uh) * 4);
	stbi_image_free(pixels);
	return true;
}

void ThumbnailCache::ensureWorker() {
	if (m_worker.joinable()) {
		return;
	}
	m_stop.store(false, std::memory_order_release);
	m_worker = std::thread([this]() { workerLoop(); });
}

void ThumbnailCache::stopWorker() {
	m_stop.store(true, std::memory_order_release);
	m_cv.notify_all();
	if (m_worker.joinable()) {
		m_worker.join();
	}
}

void ThumbnailCache::workerLoop() {
	while (!m_stop.load(std::memory_order_acquire)) {
		std::filesystem::path path;
		std::uint64_t gen = 0;
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cv.wait(lock, [this]() {
				return m_stop.load(std::memory_order_acquire) || !m_queue.empty();
			});
			if (m_stop.load(std::memory_order_acquire)) {
				return;
			}
			path = std::move(m_queue.back());
			m_queue.pop_back();
			const std::string key = pathKey(path);
			m_queued.erase(key);
			gen = m_generation.load(std::memory_order_acquire);
		}

		Raster raster;
		raster.key = pathKey(path);
		raster.generation = gen;
		if (!decodeFile(path, raster)) {
			raster.failed = true;
			raster.rgba.clear();
			raster.w = 0;
			raster.h = 0;
		}

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (raster.generation == m_generation.load(std::memory_order_acquire)) {
				m_ready.push_back(std::move(raster));
			}
		}
	}
}

void ThumbnailCache::destroyEntry(Entry& entry) {
	if (entry.tex != 0) {
		GLuint name = entry.tex;
		glDeleteTextures(1, &name);
		entry.tex = 0;
	}
}

void ThumbnailCache::evictIfNeeded() {
	while (m_lru.size() > static_cast<size_t>(kMaxEntries)) {
		const std::string oldest = m_lru.front();
		m_lru.erase(m_lru.begin());
		auto it = m_entries.find(oldest);
		if (it != m_entries.end()) {
			destroyEntry(it->second);
			m_entries.erase(it);
		}
	}
}

ImTextureID ThumbnailCache::get(const std::filesystem::path& fullPath) {
	if (m_shutdown || fullPath.empty() || !canDecode(fullPath)) {
		return ImTextureID_Invalid;
	}

	const std::string key = pathKey(fullPath);
	auto found = m_entries.find(key);
	if (found != m_entries.end()) {
		auto lruIt = std::find(m_lru.begin(), m_lru.end(), key);
		if (lruIt != m_lru.end()) {
			m_lru.erase(lruIt);
			m_lru.push_back(key);
		}
		if (found->second.failed || found->second.tex == 0) {
			return ImTextureID_Invalid;
		}
		return static_cast<ImTextureID>(static_cast<std::uintptr_t>(found->second.tex));
	}

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_queued.find(key) == m_queued.end()) {
			m_queue.push_back(fullPath);
			m_queued.insert(key);
			ensureWorker();
			m_cv.notify_one();
		}
	}
	return ImTextureID_Invalid;
}

void ThumbnailCache::tick() {
	if (m_shutdown) {
		return;
	}

	std::vector<Raster> ready;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const int n = std::min(kUploadsPerTick, static_cast<int>(m_ready.size()));
		if (n > 0) {
			ready.assign(m_ready.begin(), m_ready.begin() + n);
			m_ready.erase(m_ready.begin(), m_ready.begin() + n);
		}
	}

	const std::uint64_t gen = m_generation.load(std::memory_order_acquire);
	for (Raster& raster : ready) {
		if (raster.generation != gen || m_entries.find(raster.key) != m_entries.end()) {
			continue;
		}

		Entry entry;
		entry.failed = raster.failed;
		if (!raster.failed && raster.w > 0 && raster.h > 0 &&
			raster.rgba.size() >=
				static_cast<size_t>(raster.w) * static_cast<size_t>(raster.h) * 4) {
			GLuint tex = 0;
			glGenTextures(1, &tex);
			glBindTexture(GL_TEXTURE_2D, tex);
			GLint unpack = 4;
			glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, raster.w, raster.h, 0, GL_RGBA,
						 GL_UNSIGNED_BYTE, raster.rgba.data());
			glPixelStorei(GL_UNPACK_ALIGNMENT, unpack);
			glBindTexture(GL_TEXTURE_2D, 0);
			entry.tex = tex;
		} else {
			entry.failed = true;
		}

		m_entries.emplace(raster.key, entry);
		m_lru.push_back(raster.key);
		evictIfNeeded();
	}
}

void ThumbnailCache::clear() {
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_generation.fetch_add(1, std::memory_order_acq_rel);
		m_queue.clear();
		m_queued.clear();
		m_ready.clear();
	}
	for (auto& kv : m_entries) {
		destroyEntry(kv.second);
	}
	m_entries.clear();
	m_lru.clear();
}

void ThumbnailCache::shutdown() {
	if (m_shutdown) {
		return;
	}
	stopWorker();
	clear();
	m_shutdown = true;
}

} // namespace rigkit
