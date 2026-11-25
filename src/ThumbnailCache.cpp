#include "ThumbnailCache.h"

#include "rendering/U_gladGlfw.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif
#ifndef FILE_ATTRIBUTE_RECALL_ON_OPEN
#define FILE_ATTRIBUTE_RECALL_ON_OPEN 0x00040000
#endif
#endif

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
constexpr int kUploadsPerTick = 8;
constexpr int kMaxFailAttempts = 3;
constexpr auto kFailRetry = std::chrono::seconds(2);
// Header window. JPEG marker segments are at most 64 KB, so this reaches SOF
// past a large XMP block and the EXIF thumbnail (it lives inside one APP1).
constexpr std::uint64_t kHeaderRead = 2ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaxFullBytes = 48ull * 1024ull * 1024ull;
constexpr std::int64_t kMaxPixels = 24ll * 1024ll * 1024ll;
constexpr std::uint64_t kAbsoluteMaxBytes = 512ull * 1024ull * 1024ull;

bool readU16(const unsigned char* p, size_t n, bool be, size_t off, int& v) {
	if (off + 2 > n) {
		return false;
	}
	v = be ? ((p[off] << 8) | p[off + 1]) : (p[off] | (p[off + 1] << 8));
	return true;
}

bool readU32(const unsigned char* p, size_t n, bool be, size_t off, std::uint32_t& v) {
	if (off + 4 > n) {
		return false;
	}
	if (be) {
		v = (std::uint32_t(p[off]) << 24) | (std::uint32_t(p[off + 1]) << 16) |
			(std::uint32_t(p[off + 2]) << 8) | std::uint32_t(p[off + 3]);
	} else {
		v = std::uint32_t(p[off]) | (std::uint32_t(p[off + 1]) << 8) |
			(std::uint32_t(p[off + 2]) << 16) | (std::uint32_t(p[off + 3]) << 24);
	}
	return true;
}

// JPEGInterchangeFormat (0x0201) plus its length (0x0202), in this IFD or a child.
bool tiffFindThumb(const unsigned char* tiff, size_t n, bool be, size_t ifd, int depth,
				   std::uint32_t& outOff, std::uint32_t& outLen) {
	if (depth > 6) {
		return false;
	}
	int count = 0;
	if (!readU16(tiff, n, be, ifd, count) || count < 0 || count > 512) {
		return false;
	}
	const size_t base = ifd + 2;
	const size_t bytes = static_cast<size_t>(count) * 12u;
	if (base + bytes + 4 > n) {
		return false;
	}
	std::uint32_t off = 0;
	std::uint32_t len = 0;
	std::uint32_t exif = 0;
	for (int i = 0; i < count; ++i) {
		const size_t e = base + static_cast<size_t>(i) * 12u;
		int tag = 0;
		int type = 0;
		std::uint32_t num = 0;
		std::uint32_t raw = 0;
		if (!readU16(tiff, n, be, e, tag) || !readU16(tiff, n, be, e + 2, type) ||
			!readU32(tiff, n, be, e + 4, num) || !readU32(tiff, n, be, e + 8, raw) || num != 1) {
			continue;
		}
		if (tag == 0x0201) {
			off = raw;
		} else if (tag == 0x0202) {
			// SHORT is left-justified in the 4-byte value slot.
			len = (type == 3) ? (be ? (raw >> 16) & 0xFFFFu : raw & 0xFFFFu) : raw;
		} else if (tag == 0x8769) {
			exif = raw;
		}
	}
	if (off > 0 && len > 32 && static_cast<size_t>(off) + static_cast<size_t>(len) <= n &&
		tiff[off] == 0xFF && tiff[off + 1] == 0xD8) {
		outOff = off;
		outLen = len;
		return true;
	}
	if (exif != 0 && exif != ifd && tiffFindThumb(tiff, n, be, exif, depth + 1, outOff, outLen)) {
		return true;
	}
	std::uint32_t next = 0;
	if (!readU32(tiff, n, be, base + bytes, next) || next == 0 || next == ifd || next == exif) {
		return false;
	}
	return tiffFindThumb(tiff, n, be, next, depth + 1, outOff, outLen);
}

bool exifThumbAt(const unsigned char* tiff, size_t n, std::vector<unsigned char>& out) {
	if (n < 8) {
		return false;
	}
	bool be = false;
	if (tiff[0] == 'I' && tiff[1] == 'I') {
		be = false;
	} else if (tiff[0] == 'M' && tiff[1] == 'M') {
		be = true;
	} else {
		return false;
	}
	int magic = 0;
	std::uint32_t ifd0 = 0;
	if (!readU16(tiff, n, be, 2, magic) || magic != 42 || !readU32(tiff, n, be, 4, ifd0)) {
		return false;
	}
	std::uint32_t off = 0;
	std::uint32_t len = 0;
	if (!tiffFindThumb(tiff, n, be, ifd0, 0, off, len)) {
		return false;
	}
	out.assign(tiff + off, tiff + off + len);
	return true;
}

// One pass over a JPEG header: pixel count from SOF, embedded EXIF thumbnail if present.
void scanJpeg(const unsigned char* data, size_t n, bool& havePixels, std::int64_t& pixels,
			  std::vector<unsigned char>& thumb) {
	havePixels = false;
	pixels = 0;
	thumb.clear();
	if (n < 4 || data[0] != 0xFF || data[1] != 0xD8) {
		return;
	}
	size_t i = 2;
	while (i + 1 < n) {
		if (data[i] != 0xFF) {
			return;
		}
		++i;
		while (i < n && data[i] == 0xFF) {
			++i;
		}
		if (i >= n) {
			return;
		}
		const unsigned char marker = data[i++];
		if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
			continue;
		}
		if (marker == 0xD9 || marker == 0xDA) {
			return;
		}
		if (i + 2 > n) {
			return;
		}
		const int len = (data[i] << 8) | data[i + 1];
		if (len < 2) {
			return;
		}
		const size_t seg = static_cast<size_t>(len);
		if (i + seg > n) {
			return;
		}
		const unsigned char* payload = data + i + 2;
		const size_t payloadN = seg - 2;
		const bool sof =
			marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
		if (sof && payloadN >= 5) {
			const int h = (payload[1] << 8) | payload[2];
			const int w = (payload[3] << 8) | payload[4];
			if (w > 0 && h > 0) {
				havePixels = true;
				pixels = static_cast<std::int64_t>(w) * static_cast<std::int64_t>(h);
			}
		} else if (marker == 0xE1 && thumb.empty() && payloadN > 6 &&
				   std::memcmp(payload, "Exif\0\0", 6) == 0) {
			std::vector<unsigned char> embedded;
			if (exifThumbAt(payload + 6, payloadN - 6, embedded)) {
				thumb.swap(embedded);
			}
		}
		i += seg;
		if (havePixels && !thumb.empty()) {
			return;
		}
	}
}

bool readExact(std::ifstream& in, unsigned char* dst, std::uint64_t n) {
	if (n == 0) {
		return true;
	}
	if (n > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
		return false;
	}
	in.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(n));
	return static_cast<std::uint64_t>(in.gcount()) == n;
}

// Online-only / defer-hydrate files. Opening them for a preview can stall the
// whole process while the provider fetches bytes - skip and keep the type plate.
bool isDeferredHydrate(const std::filesystem::path& path) {
#ifdef _WIN32
	const DWORD attrs = GetFileAttributesW(path.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES) {
		return false;
	}
	constexpr DWORD kDefer = FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS |
							 FILE_ATTRIBUTE_RECALL_ON_OPEN;
	return (attrs & kDefer) != 0;
#else
	(void)path;
	return false;
#endif
}

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

void ThumbnailCache::registerDecoder(const std::string& extension,
									 IMui::FileThumbnailDecoder decoder) {
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

bool ThumbnailCache::decodeMemory(const unsigned char* data, int len, Raster& out) {
	if (!data || len < 16) {
		return false;
	}
	int infoW = 0;
	int infoH = 0;
	int infoComp = 0;
	if (!stbi_info_from_memory(data, len, &infoW, &infoH, &infoComp) || infoW < 1 || infoH < 1) {
		return false;
	}
	const std::int64_t pixelCount =
		static_cast<std::int64_t>(infoW) * static_cast<std::int64_t>(infoH);
	if (pixelCount > kMaxPixels) {
		return false;
	}

	int w = 0;
	int h = 0;
	int comp = 0;
	unsigned char* pixels = stbi_load_from_memory(data, len, &w, &h, &comp, 4);
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

bool ThumbnailCache::decodeFile(const std::filesystem::path& path, Raster& out) {
	if (isDeferredHydrate(path)) {
		return false;
	}

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

	// ifstream + path uses the native wide API on Windows; stbi_load(char*) does not.
	std::error_code ec;
	const std::uint64_t fileSize = std::filesystem::file_size(path, ec);
	if (ec || fileSize == 0 || fileSize > kAbsoluteMaxBytes) {
		return false;
	}
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return false;
	}
	const size_t headerN = static_cast<size_t>(std::min(fileSize, kHeaderRead));
	std::vector<unsigned char> header(headerN);
	if (!readExact(in, header.data(), headerN)) {
		return false;
	}

	const std::string ext = normalizeExt(path);
	const bool jpeg = ext == ".jpg" || ext == ".jpeg";
	if (jpeg) {
		bool havePixels = false;
		std::int64_t pixels = 0;
		std::vector<unsigned char> thumb;
		scanJpeg(header.data(), header.size(), havePixels, pixels, thumb);
		if (!thumb.empty() &&
			thumb.size() <= static_cast<size_t>(std::numeric_limits<int>::max()) &&
			decodeMemory(thumb.data(), static_cast<int>(thumb.size()), out)) {
			return true;
		}
		if (fileSize > kMaxFullBytes || (havePixels && pixels > kMaxPixels)) {
			return false;
		}
	} else if (fileSize > kMaxFullBytes) {
		return false;
	}

	std::vector<unsigned char> file = std::move(header);
	if (fileSize > file.size()) {
		file.resize(static_cast<size_t>(fileSize));
		if (!readExact(in, file.data() + headerN, fileSize - headerN)) {
			return false;
		}
	}
	if (file.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
		return false;
	}
	return decodeMemory(file.data(), static_cast<int>(file.size()), out);
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
				return m_stop.load(std::memory_order_acquire) ||
					   (!m_paused.load(std::memory_order_acquire) && !m_queue.empty());
			});
			if (m_stop.load(std::memory_order_acquire)) {
				return;
			}
			path = std::move(m_queue.front());
			m_queue.erase(m_queue.begin());
			const std::string key = pathKey(path);
			m_queued.erase(key);
			m_inflight = key;
			gen = m_generation.load(std::memory_order_acquire);
		}

		Raster raster;
		raster.key = pathKey(path);
		raster.generation = gen;
		bool decoded = false;
		try {
			decoded = decodeFile(path, raster);
		} catch (...) {
			decoded = false;
		}
		if (!decoded) {
			raster.failed = true;
			raster.rgba.clear();
			raster.w = 0;
			raster.h = 0;
		}

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_inflight == raster.key) {
				m_inflight.clear();
			}
			if (raster.generation == m_generation.load(std::memory_order_acquire)) {
				m_ready.push_back(std::move(raster));
			}
		}
	}
}

void ThumbnailCache::destroyEntry(Entry& entry) {
	if (entry.tex != 0) {
		m_doomed.push_back(entry.tex);
		entry.tex = 0;
	}
}

void ThumbnailCache::flushDoomed() {
	for (unsigned name : m_doomed) {
		GLuint tex = name;
		glDeleteTextures(1, &tex);
	}
	m_doomed.clear();
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

ImTextureID ThumbnailCache::get(const std::filesystem::path& fullPath, int* outW, int* outH,
								bool* pending) {
	if (outW) {
		*outW = 0;
	}
	if (outH) {
		*outH = 0;
	}
	if (pending) {
		*pending = false;
	}
	if (m_shutdown || fullPath.empty() || !canDecode(fullPath)) {
		return ImTextureID_Invalid;
	}
	// Do not queue a hydrate. pending stays false so the tile keeps the type plate.
	if (isDeferredHydrate(fullPath)) {
		return ImTextureID_Invalid;
	}

	const std::string key = pathKey(fullPath);
	m_seen.insert(key);
	if (m_skipped.find(key) != m_skipped.end()) {
		return ImTextureID_Invalid;
	}

	auto found = m_entries.find(key);
	if (found != m_entries.end()) {
		auto lruIt = std::find(m_lru.begin(), m_lru.end(), key);
		if (lruIt != m_lru.end()) {
			m_lru.erase(lruIt);
			m_lru.push_back(key);
		}
		if (!found->second.failed && found->second.tex != 0) {
			if (outW) {
				*outW = found->second.w;
			}
			if (outH) {
				*outH = found->second.h;
			}
			return static_cast<ImTextureID>(static_cast<std::uintptr_t>(found->second.tex));
		}
		// Soft-fail: re-queue after a short TTL until the attempt cap.
		const auto now = std::chrono::steady_clock::now();
		const bool retry = found->second.failed && found->second.attempts < kMaxFailAttempts &&
						   now - found->second.failedAt >= kFailRetry;
		if (!retry) {
			if (pending) {
				*pending = found->second.failed && found->second.attempts < kMaxFailAttempts;
			}
			return ImTextureID_Invalid;
		}
		// Keep the failed entry so attempts survive; tick replaces it on success/fail.
	}

	bool startWorker = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const bool busy = m_inflight == key || m_queued.find(key) != m_queued.end();
		if (!busy) {
			m_queue.push_back(fullPath);
			m_queued.insert(key);
			startWorker = !m_worker.joinable();
			m_cv.notify_one();
		}
	}
	if (startWorker) {
		ensureWorker();
	}
	if (pending) {
		*pending = true;
	}
	return ImTextureID_Invalid;
}

void ThumbnailCache::setPaused(bool paused) {
	m_paused.store(paused, std::memory_order_release);
	m_cv.notify_all();
}

bool ThumbnailCache::paused() const { return m_paused.load(std::memory_order_acquire); }

void ThumbnailCache::cancel(const std::filesystem::path& fullPath) {
	if (fullPath.empty()) {
		return;
	}
	const std::string key = pathKey(fullPath);
	m_skipped.insert(key);
	std::lock_guard<std::mutex> lock(m_mutex);
	m_queued.erase(key);
	m_queue.erase(
		std::remove_if(m_queue.begin(), m_queue.end(),
					   [&](const std::filesystem::path& path) { return pathKey(path) == key; }),
		m_queue.end());
	m_ready.erase(std::remove_if(m_ready.begin(), m_ready.end(),
								 [&](const Raster& raster) { return raster.key == key; }),
				  m_ready.end());
}

void ThumbnailCache::finishFrame() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_queue.empty()) {
		std::vector<std::filesystem::path> keep;
		keep.reserve(m_queue.size());
		for (const std::filesystem::path& path : m_queue) {
			const std::string key = pathKey(path);
			if (m_seen.find(key) != m_seen.end()) {
				keep.push_back(path);
			} else {
				m_queued.erase(key);
			}
		}
		m_queue.swap(keep);
	}
	m_seen.clear();
}

void ThumbnailCache::tick() {
	if (m_shutdown) {
		return;
	}
	flushDoomed();

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
		if (raster.generation != gen || m_skipped.find(raster.key) != m_skipped.end()) {
			continue;
		}
		auto existing = m_entries.find(raster.key);
		if (existing != m_entries.end() && !existing->second.failed && existing->second.tex != 0) {
			continue;
		}

		Entry entry;
		entry.failed = raster.failed;
		entry.w = raster.w;
		entry.h = raster.h;
		if (existing != m_entries.end()) {
			entry.attempts = existing->second.attempts;
			destroyEntry(existing->second);
			m_entries.erase(existing);
			auto lruIt = std::find(m_lru.begin(), m_lru.end(), raster.key);
			if (lruIt != m_lru.end()) {
				m_lru.erase(lruIt);
			}
		}
		if (!raster.failed && raster.w > 0 && raster.h > 0 &&
			raster.rgba.size() >=
				static_cast<size_t>(raster.w) * static_cast<size_t>(raster.h) * 4) {
			GLuint tex = 0;
			glGenTextures(1, &tex);
			glBindTexture(GL_TEXTURE_2D, tex);
			// Default unpack alignment is 4. Do not glGetIntegerv here - that
			// syncs the GL pipeline and hitch the frame while spinners run.
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, raster.w, raster.h, 0, GL_RGBA,
						 GL_UNSIGNED_BYTE, raster.rgba.data());
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glBindTexture(GL_TEXTURE_2D, 0);
			entry.tex = tex;
			entry.attempts = 0;
		} else {
			entry.failed = true;
			// A rejected file stays on the type plate. Refresh clears the cache.
			entry.attempts = kMaxFailAttempts;
			entry.failedAt = std::chrono::steady_clock::now();
			entry.w = 0;
			entry.h = 0;
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
		m_inflight.clear();
		m_ready.clear();
	}
	m_skipped.clear();
	m_seen.clear();
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
	flushDoomed();
	m_shutdown = true;
}

} // namespace rigkit
