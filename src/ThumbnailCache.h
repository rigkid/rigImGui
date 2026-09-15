#pragma once

/**
 * @file
 * @brief Small GLES thumbnails for the Open/Save file browser Icons view.
 * @details Decode runs on a worker; `tick()` uploads a few textures per frame.
 * Destroy via `shutdown()` while the GL context is alive (before ImGui teardown).
 */

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/IMui.h"

#include <imgui.h>

namespace rigkit {

class ThumbnailCache {
  public:
	ThumbnailCache();
	~ThumbnailCache();

	ThumbnailCache(const ThumbnailCache&) = delete;
	ThumbnailCache& operator=(const ThumbnailCache&) = delete;

	/** @brief Pack-registered CPU decoder. Extension is stored lowercased. */
	void registerDecoder(const std::string& extension, IMui::FileThumbnailDecoder decoder);

	/** @brief Cached texture, or 0 (and a queued decode) when not ready. */
	ImTextureID get(const std::filesystem::path& fullPath);
	/** @brief Upload a few ready rasters. Call on the GL thread before Display. */
	void tick();
	/** @brief Drop queued work and GPU thumbs. GL context must be current. */
	void clear();
	/** @brief Stop the worker and delete textures. Call before GL shutdown. */
	void shutdown();

  private:
	struct Raster {
		std::string key;
		std::uint64_t generation = 0;
		int w = 0;
		int h = 0;
		bool failed = false;
		std::vector<std::uint8_t> rgba;
	};

	struct Entry {
		unsigned tex = 0;
		bool failed = false;
	};

	static std::string normalizeExt(const std::filesystem::path& path);
	static std::string pathKey(const std::filesystem::path& path);
	static std::string pathUtf8(const std::filesystem::path& path);
	static void boxDownsample(const unsigned char* src, int sw, int sh, unsigned char* dst, int dw,
							  int dh);
	bool canDecode(const std::filesystem::path& path) const;
	IMui::FileThumbnailDecoder decoderFor(const std::string& ext) const;
	bool decodeFile(const std::filesystem::path& path, Raster& out);

	void ensureWorker();
	void stopWorker();
	void workerLoop();
	void destroyEntry(Entry& entry);
	void evictIfNeeded();

	std::unordered_map<std::string, Entry> m_entries;
	std::vector<std::string> m_lru;

	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	std::vector<std::filesystem::path> m_queue;
	std::unordered_set<std::string> m_queued;
	std::vector<Raster> m_ready;
	std::thread m_worker;
	std::atomic<bool> m_stop{false};
	std::atomic<std::uint64_t> m_generation{1};
	bool m_shutdown = false;
	std::unordered_map<std::string, IMui::FileThumbnailDecoder> m_decoders;
};

} // namespace rigkit
