#include "TtfKern.h"

#include <cstring>
#include <fstream>
#include <vector>

namespace rigkit {
namespace {

uint16_t ru16(const uint8_t* p) {
	return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

int16_t ri16(const uint8_t* p) {
	return static_cast<int16_t>(ru16(p));
}

uint32_t ru32(const uint8_t* p) {
	return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

bool inRange(size_t size, uint32_t offset, uint32_t need) {
	return need <= size && offset <= size - need;
}

bool findTable(const uint8_t* data, size_t size, uint32_t tag, uint32_t& offset, uint32_t& length) {
	if (!inRange(size, 0, 12)) {
		return false;
	}
	const uint16_t n = ru16(data + 4);
	if (!inRange(size, 12, uint32_t(n) * 16u)) {
		return false;
	}
	for (uint16_t i = 0; i < n; ++i) {
		const uint8_t* e = data + 12 + i * 16;
		if (ru32(e) == tag) {
			offset = ru32(e + 8);
			length = ru32(e + 12);
			return inRange(size, offset, length);
		}
	}
	return false;
}

uint64_t pairKey(unsigned left, unsigned right) {
	return (uint64_t(left) << 32) | uint64_t(right);
}

} // namespace

void TtfKern::clear() {
	m_unitsPerEm = 1000;
	m_pairs.clear();
}

bool TtfKern::loadFromFile(const std::string& path) {
	clear();
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return false;
	}
	in.seekg(0, std::ios::end);
	const auto n = in.tellg();
	if (n <= 0) {
		return false;
	}
	in.seekg(0, std::ios::beg);
	std::vector<uint8_t> buf(static_cast<size_t>(n));
	in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
	if (!in) {
		return false;
	}
	return parse(buf.data(), buf.size());
}

bool TtfKern::loadFromMemory(const void* data, size_t size) {
	clear();
	if (!data || size == 0) {
		return false;
	}
	return parse(static_cast<const uint8_t*>(data), size);
}

float TtfKern::pairPx(unsigned left, unsigned right, float sizePx) const {
	if (m_pairs.empty() || m_unitsPerEm == 0) {
		return 0.f;
	}
	const auto it = m_pairs.find(pairKey(left, right));
	if (it == m_pairs.end()) {
		return 0.f;
	}
	return float(it->second) * sizePx / float(m_unitsPerEm);
}

bool TtfKern::parse(const uint8_t* data, size_t size) {
	uint32_t headOff = 0, headLen = 0;
	if (findTable(data, size, 0x68656164u, headOff, headLen) && headLen >= 20) {
		m_unitsPerEm = ru16(data + headOff + 18);
		if (m_unitsPerEm == 0) {
			m_unitsPerEm = 1000;
		}
	}

	uint32_t cmapOff = 0, cmapLen = 0;
	if (!findTable(data, size, 0x636d6170u, cmapOff, cmapLen)) {
		return false;
	}
	std::unordered_map<uint16_t, uint32_t> gidToUni;
	if (!readCmap(data, size, cmapOff, cmapLen, gidToUni)) {
		return false;
	}

	uint32_t kernOff = 0, kernLen = 0;
	if (!findTable(data, size, 0x6b65726eu, kernOff, kernLen)) {
		return false;
	}
	return readKern(data, size, kernOff, kernLen, gidToUni, m_pairs);
}

bool TtfKern::readCmap(const uint8_t* data, size_t size, uint32_t offset, uint32_t length,
					   std::unordered_map<uint16_t, uint32_t>& gidToUni) {
	if (length < 4 || !inRange(size, offset, length)) {
		return false;
	}
	const uint8_t* cmap = data + offset;
	const uint16_t nEnc = ru16(cmap + 2);
	if (length < 4u + uint32_t(nEnc) * 8u) {
		return false;
	}

	uint32_t fmtOff = 0;
	for (uint16_t i = 0; i < nEnc; ++i) {
		const uint8_t* rec = cmap + 4 + i * 8;
		const uint16_t plat = ru16(rec);
		const uint16_t enc = ru16(rec + 2);
		const uint32_t sub = ru32(rec + 4);
		if (sub + 2 > length) {
			continue;
		}
		const uint16_t fmt = ru16(cmap + sub);
		if (fmt != 4) {
			continue;
		}
		// Prefer Windows BMP (3,1), else Unicode (0,*).
		if (plat == 3 && enc == 1) {
			fmtOff = sub;
			break;
		}
		if (plat == 0 && fmtOff == 0) {
			fmtOff = sub;
		}
	}
	if (fmtOff == 0) {
		return false;
	}

	const uint8_t* t = cmap + fmtOff;
	if (fmtOff + 14 > length) {
		return false;
	}
	const uint16_t segCountX2 = ru16(t + 6);
	if ((segCountX2 & 1u) != 0) {
		return false;
	}
	const uint16_t segCount = segCountX2 / 2;
	const uint32_t need = 16u + uint32_t(segCount) * 8u;
	if (fmtOff + need > length) {
		return false;
	}

	const uint8_t* endCode = t + 14;
	const uint8_t* startCode = endCode + segCountX2 + 2;
	const uint8_t* idDelta = startCode + segCountX2;
	const uint8_t* idRangeOffset = idDelta + segCountX2;
	const uint8_t* glyphIdArray = idRangeOffset + segCountX2;

	for (uint16_t s = 0; s < segCount; ++s) {
		const uint16_t start = ru16(startCode + s * 2);
		const uint16_t end = ru16(endCode + s * 2);
		const int16_t delta = ri16(idDelta + s * 2);
		const uint16_t ro = ru16(idRangeOffset + s * 2);
		for (uint32_t c = start; c <= end; ++c) {
			uint16_t gid = 0;
			if (ro == 0) {
				gid = static_cast<uint16_t>(c + delta);
			} else {
				const uint32_t idx = uint32_t(ro) / 2u + (c - start) + s;
				const uint8_t* gp = idRangeOffset + idx * 2u;
				if (gp < glyphIdArray || gp + 2 > cmap + length) {
					continue;
				}
				const uint16_t raw = ru16(gp);
				if (raw != 0) {
					gid = static_cast<uint16_t>(raw + delta);
				}
			}
			if (gid != 0) {
				gidToUni.emplace(gid, c);
			}
		}
	}
	return !gidToUni.empty();
}

bool TtfKern::readKern(const uint8_t* data, size_t size, uint32_t offset, uint32_t length,
					   const std::unordered_map<uint16_t, uint32_t>& gidToUni,
					   std::unordered_map<uint64_t, int16_t>& pairs) {
	if (length < 4 || !inRange(size, offset, length)) {
		return false;
	}
	const uint8_t* kern = data + offset;
	const uint16_t version = ru16(kern);
	uint16_t nTables = ru16(kern + 2);
	uint32_t cursor = 4;
	// Apple kern v1 stores a 32-bit version and table count.
	if (version == 1 && length >= 8) {
		nTables = ru16(kern + 6);
		cursor = 8;
	}

	for (uint16_t t = 0; t < nTables && cursor + 6 <= length; ++t) {
		const uint8_t* sub = kern + cursor;
		const uint16_t subLen = ru16(sub + 2);
		const uint16_t coverage = ru16(sub + 4);
		const uint16_t format = coverage >> 8;
		if (format == 0 && cursor + 14 <= length) {
			const uint16_t nPairs = ru16(sub + 6);
			const uint32_t pairBytes = uint32_t(nPairs) * 6u;
			if (cursor + 14 + pairBytes > length) {
				break;
			}
			const uint8_t* p = sub + 14;
			for (uint16_t i = 0; i < nPairs; ++i, p += 6) {
				const uint16_t leftG = ru16(p);
				const uint16_t rightG = ru16(p + 2);
				const int16_t value = ri16(p + 4);
				if (value == 0) {
					continue;
				}
				const auto leftIt = gidToUni.find(leftG);
				const auto rightIt = gidToUni.find(rightG);
				if (leftIt == gidToUni.end() || rightIt == gidToUni.end()) {
					continue;
				}
				pairs.emplace(pairKey(leftIt->second, rightIt->second), value);
			}
			return !pairs.empty();
		}
		if (subLen < 6) {
			break;
		}
		cursor += subLen;
	}
	return !pairs.empty();
}

} // namespace rigkit
