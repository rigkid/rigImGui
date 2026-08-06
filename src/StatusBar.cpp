#include "StatusBar.h"
#include <algorithm>

namespace rigkit {

void StatusBar::clear() {
	m_slots.clear();
	m_left.clear();
}

void StatusBar::setSlot(StatusSlot slot) {
	removeSlot(slot.id);
	m_slots.push_back(std::move(slot));
}

void StatusBar::removeSlot(const std::string& id) {
	m_slots.erase(std::remove_if(m_slots.begin(), m_slots.end(),
								 [&](const StatusSlot& s) { return s.id == id; }),
				  m_slots.end());
}

} // namespace rigkit
