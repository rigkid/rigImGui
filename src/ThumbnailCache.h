#pragma once

/**
 * @file
 * @brief Small GLES thumbnails for the Open/Save file browser Icons view.
 * @details Decode runs on one worker. `tick()` uploads a few textures per frame.
 * A JPEG with an embedded EXIF thumbnail uses that. Otherwise the full image is
 * decoded only up to 24 megapixels and 48 MB. Larger files without an embedded
 * thumbnail stay on the type plate. Online-only / defer-hydrate files (OneDrive
 * and similar) are skipped so opening a preview cannot stall the UI process.
 * `setPaused` holds the queue. `cancel` drops one path until `clear`.
 * `finishFrame` drops queued paths not requested since the previous call.
 * The in-flight decode runs to completion; stb cannot be interrupted.
 * Destroy via `shutdown()` while the GL context is alive (before ImGui teardown).
 */

#include <atomic>
#include <chrono>
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

	/**
	 * @brief Cached texture, or invalid when the thumb is not ready.
	 * @param outW Pixel width when a texture is returned.
	 * @param outH Pixel height when a texture is returned.
	 * @param pending Set when this path is queued, decoding, or waiting to retry.
	 *        Left false when a texture is ready, the path was cancelled, or decode gave up.
	 */
	ImTextureID get(const std::filesystem::path& fullPath, int* outW = nullptr, int* outH = nullptr,
					bool* pending = nullptr);
	/** @brief Hold the queue. Queued paths stay pending until resumed. */
	void setPaused(bool paused);
	/** @brief True while `setPaused(true)` is in effect. */
	bool paused() const;
	/**
	 * @brief Drop this path until `clear()` (Refresh or dialog close).
	 * @details An in-flight decode is discarded when it finishes.
	 */
	void cancel(const std::filesystem::path& fullPath);
	/**
	 * @brief Drop queued paths that `get` did not touch since the previous call.
	 * @details Call once a frame after the Icons view has drawn. Paths that scrolled
	 * out of view, and the queue when the dialog is closed, are not decoded.
	 */
	void finishFrame();
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
		int w = 0;
		int h = 0;
		bool failed = false;
		int attempts = 0;
		std::chrono::steady_clock::time_point failedAt{};
	};

	static std::string normalizeExt(const std::filesystem::path& path);
	static std::string pathKey(const std::filesystem::path& path);
	static std::string pathUtf8(const std::filesystem::path& path);
	static void boxDownsample(const unsigned char* src, int sw, int sh, unsigned char* dst, int dw,
							  int dh);
	bool canDecode(const std::filesystem::path& path) const;
	IMui::FileThumbnailDecoder decoderFor(const std::string& ext) const;
	bool decodeMemory(const unsigned char* data, int len, Raster& out);
	bool decodeFile(const std::filesystem::path& path, Raster& out);

	void ensureWorker();
	void stopWorker();
	void workerLoop();
	void destroyEntry(Entry& entry);
	void evictIfNeeded();
	void flushDoomed();

	std::unordered_map<std::string, Entry> m_entries;
	std::vector<std::string> m_lru;
	std::vector<unsigned> m_doomed;

	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	std::vector<std::filesystem::path> m_queue;
	std::unordered_set<std::string> m_queued;
	std::string m_inflight;
	std::vector<Raster> m_ready;
	std::atomic<bool> m_paused{false};
	// UI thread only. cancel() until clear().
	std::unordered_set<std::string> m_skipped;
	// Paths get() saw since the previous finishFrame(). UI thread only.
	std::unordered_set<std::string> m_seen;
	std::thread m_worker;
	std::atomic<bool> m_stop{false};
	std::atomic<std::uint64_t> m_generation{1};
	bool m_shutdown = false;
	std::unordered_map<std::string, IMui::FileThumbnailDecoder> m_decoders;
};

} // namespace rigkit
