#include "MWindow.h"
#include <spdlog/spdlog.h>

namespace rigkit {

void MWindow::showWindow(const std::string &title) {
	if (auto window = getWindow<IWindow>(title)) {
		window->setVisible(true);
	}
}

void MWindow::hideWindow(const std::string &title) {
	if (auto window = getWindow<IWindow>(title)) {
		window->setVisible(false);
	}
}

void MWindow::setWindowVisible(const std::string &title, bool visible) {
	if (auto window = getWindow<IWindow>(title)) {
		window->setVisible(visible);
	}
}

void MWindow::toggleWindow(const std::string &title) {
	if (auto window = getWindow<IWindow>(title)) {
		window->setVisible(!window->isVisible());
	}
}

void MWindow::showAllWindows() {
	for (auto &pair : m_windowMap) {
		pair.second->setVisible(true);
	}
}

void MWindow::hideAllWindows() {
	for (auto &pair : m_windowMap) {
		pair.second->setVisible(false);
	}
}

void MWindow::renderAllWindows() {
	for (auto &pair : m_windowMap) {
		pair.second->render();
	}
}

void MWindow::removeWindow(const std::string &title) {
	m_windowMap.erase(title);
}

void MWindow::clear() { m_windowMap.clear(); }

size_t MWindow::getWindowCount() const { return m_windowMap.size(); }

bool MWindow::getWindowVisibility(const std::string &title) {
	if (auto window = getWindow<IWindow>(title)) {
		return window->isVisible();
	}
	return false;
}

std::vector<std::string> MWindow::getAllWindowNames() const {
	std::vector<std::string> names;
	for (const auto &pair : m_windowMap) {
		names.push_back(pair.first);
	}
	return names;
}

} // namespace rigkit
