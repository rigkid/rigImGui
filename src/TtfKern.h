#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace rigkit {

/**
 * @brief Legacy TTF `kern` format-0 pairs, keyed by Unicode.
 * @details ImGui atlas text has advance only. This table supplies pair extras
 * at draw time. GPOS / class kerning is not read — bind
 * `IMui::setChromeKernFn` (VarFont `GetGposPairExtraPx`) for that.
 */
class TtfKern {
  public:
	void clear();
	bool loadFromFile(const std::string& path);
	bool loadFromMemory(const void* data, size_t size);

	int pairCount() const { return static_cast<int>(m_pairs.size()); }
	uint16_t unitsPerEm() const { return m_unitsPerEm; }

	/// Extra advance in pixels at `sizePx`. 0 when the pair is absent.
	float pairPx(unsigned left, unsigned right, float sizePx) const;

  private:
	bool parse(const uint8_t* data, size_t size);
	static bool readCmap(const uint8_t* data, size_t size, uint32_t offset, uint32_t length,
						 std::unordered_map<uint16_t, uint32_t>& gidToUni);
	static bool readKern(const uint8_t* data, size_t size, uint32_t offset, uint32_t length,
						 const std::unordered_map<uint16_t, uint32_t>& gidToUni,
						 std::unordered_map<uint64_t, int16_t>& pairs);

	uint16_t m_unitsPerEm = 1000;
	std::unordered_map<uint64_t, int16_t> m_pairs;
};

} // namespace rigkit
