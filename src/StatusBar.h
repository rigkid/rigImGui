#pragma once

#include <functional>
#include <string>
#include <vector>

namespace rigkit {

/**
 * @brief Bottom status strip slots - data/callbacks only until drawn by Mui.
 */
struct StatusSlot {
	std::string id;
	std::function<std::string()> text;
	float width = 0.f;			   ///< 0 = auto (72 when text-only)
	std::function<void()> draw;	   ///< If set, drawn instead of text
};

class StatusBar {
  public:
	void clear();
	void setSlot(StatusSlot slot);
	void removeSlot(const std::string& id);
	const std::vector<StatusSlot>& slots() const { return m_slots; }

	void setLeft(std::string text) { m_left = std::move(text); }
	const std::string& left() const { return m_left; }

  private:
	std::vector<StatusSlot> m_slots;
	std::string m_left;
};

} // namespace rigkit
