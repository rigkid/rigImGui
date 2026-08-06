#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "IWindow.h"

// Forward declarations
namespace rigkit {
class MEcs;
class RigKitEngine;
} // namespace rigkit

namespace rigkit {

class MWindow {
  public:
	MWindow() = default;
	~MWindow() = default;

	// Engine access
	void setEngine(RigKitEngine *engine) { m_engine = engine; }
	RigKitEngine *getEngine() const { return m_engine; }

	// Window management
	template <typename T, typename... Args>
	std::shared_ptr<T> createWindow(Args &&...args) {
		auto window = std::make_shared<T>(std::forward<Args>(args)...);
		window->setEngine(m_engine); // Set engine access
		m_windowMap[window->getTitle()] = window;
		return window;
	}

	// Get specific typed window (including IWindow for base interface)
	template <typename T>
	std::shared_ptr<T> getWindow(const std::string &title) {
		auto it = m_windowMap.find(title);
		if (it != m_windowMap.end()) {
			return std::dynamic_pointer_cast<T>(it->second);
		}
		return nullptr;
	}

	void showWindow(const std::string &title);
	void hideWindow(const std::string &title);
	void setWindowVisible(const std::string &title, bool visible);
	void toggleWindow(const std::string &title);
	void showAllWindows();
	void hideAllWindows();
	void renderAllWindows();
	void removeWindow(const std::string &title);
	void clear();
	size_t getWindowCount() const;
	std::vector<std::string> getAllWindowNames() const;
	bool getWindowVisibility(const std::string &title);

  private:
	std::unordered_map<std::string, std::shared_ptr<IWindow>> m_windowMap;
	RigKitEngine *m_engine = nullptr;
};

} // namespace rigkit
