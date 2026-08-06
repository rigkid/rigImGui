#pragma once

#include <imgui.h>
#include <string>

// Forward declaration
namespace rigkit {
class RigKitEngine;
}

namespace rigkit {

class IWindow {
  public:
	IWindow(const std::string& title, ImGuiWindowFlags flags = 0);
	virtual ~IWindow() = default;

	// Engine access - allows windows to access managers through normal RigKit
	// patterns
	void setEngine(RigKitEngine* engine);
	RigKitEngine* getEngine() const;

	// Window management - in immediate mode, these are controlled by ECS
	bool isVisible() const;
	void setVisible(bool visible);

	// Window properties
	void setTitle(const std::string& title);
	std::string getTitle() const;

	// Additional ImGui-specific methods
	bool isOpen() const;
	bool isFocused() const;

	// Category management (for organization)
	std::string getCategory() const;
	void setCategory(const std::string& category);

	// Rendering
	virtual void render();

	void handleInput();

  protected:
	// Derived classes only implement the window's UI here
	virtual void renderContents() = 0;
	std::string m_title;
	bool m_isOpen = true;
	bool m_isVisible = true;
	bool m_focusOnShow = false;
	const ImGuiWindowFlags m_windowFlags;
	std::string m_category = "General";
	RigKitEngine* m_engine = nullptr;
};

} // namespace rigkit
