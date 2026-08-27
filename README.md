# rigImGui


![preview](examples/host_shell/img/preview.png)


**`rigImGui`** is the default RigKit UI pack.

```bash
cmake -S examples/host_shell -B examples/host_shell/build
cmake --build examples/host_shell/build --target host_shell
```

Also: [sample_menubar](examples/sample_menubar/), [example_filebrowser](examples/example_filebrowser/), [example_ImGuizmo](examples/example_ImGuizmo/).

It fulfills UI via `IMui` — part of a **SUDE–ECS–UI fulfillment**, not SUDE alone. Show/headless hosts may omit it and remain SUDE or SUDE–ECS.

Dear ImGui docking is a submodule of [GitBruno/imgui](https://github.com/GitBruno/imgui) (`features/font-kerning-fn-v1.92.1`) — v1.92.1 plus optional `ImFont::KerningFn` ([ocornut/imgui#9516](https://github.com/ocornut/imgui/pull/9516)). After that PR merges, retarget `third_party/imgui` to `ocornut/imgui` `@ docking`. Other `third_party/` trees stay vendored.

```bash
git submodule update --init --recursive
```

The pack extends RigKit with a complete UI system built on Dear ImGui. It provides:

- **Window Management**: Create, manage, and organize ImGui windows
- **Host shell**: Edit menu + undo bind, Edit Mode (opt-in via `enableEditMode(true)`; panels hidden until Ctrl+E), Scene (DnD reparent), Layers (`CLayer`), Viewport (`View2D`), rulers (F2), 2D handles, Tools W·E·R gizmo mode, File→Export PNG, shortcuts panel with remapping (persisted in user settings), status bar, open/save dialogs
- **Standard Windows**: Pre-built windows for common tasks (debug, properties, logs)
- **Custom UI**: Easy creation of custom windows and UI components
- **ECS Integration**: Seamless integration with RigKit's Entity-Component-System
- **Engine Access**: Windows can access all RigKit managers through the engine

Apps wire 3D gizmos with `Mui::setGizmoDrawer` (see `example-lowpoly`). Undo history stays outside UI — `IMui::setUndoStack`.

## Core Components

### `IWindow` - Window Interface
The base interface for all ImGui windows. Provides:
- Immediate mode rendering with `render()` method
- Visibility control with `setVisible()` and `isVisible()`
- Engine access through `setEngine()` and `getEngine()`

### `MWindow` - Window Manager
Manages all `IWindow` instances:
- Window creation and destruction
- Visibility management
- Workspace save/load functionality
- Engine integration for ECS access

### `Mui` - UI Manager
The main UI system that orchestrates everything:
- ImGui initialization and shutdown
- Theme / font / style extras via **`ImGuiStyleKit`**
- Notification and modal system
- Event system integration
- Window manager coordination

### `ImGuiStyleKit` - Themes and fonts
- Metrics, borders, rounding, and Dark / Light bases
- Built-in bases: Dark (default charcoal/teal) and Light. Named palettes are JSON in `themes/`
- **Color-scheme JSON** — shipped `themes/` plus user saves under `<userData>/user/themes/`
- **Theme panel** — Dark / Light + scheme combo with credits (full Style Editor is Preferences)
- **Full Style Editor** — Preferences → Interface → `ImGui::ShowStyleEditor()` (Dear ImGui stock)
- **Custom font** — Theme panel / Preferences: TTF path (under `data/fonts` or absolute) + size; Apply Font reloads atlas
- Prefs (`rigImGui.ui`): Theme, Theme File, Font File, Font Size — File → Preferences…
- **Roboto** + **Font Awesome 5** shipped under `fonts/` (also copied to repo `assets/fonts/` for `AppPaths`)

## Standard Windows

Built-in host panels are created on demand — ask for the ones you need:

| `HostPanel` | Window |
|-------------|--------|
| `Log` | Scrollable log with filtering |
| `Windows` | Window visibility manager |
| `Debug` | Debugging tools |
| `Properties` | ECS component property editor |
| `Scene` | Entity scene tree |
| `Layers` | Layer list |
| `Viewport` | GL viewport panel |
| `Shortcuts` | Shortcut list + click-to-remap (persists under settings key `shortcuts`) |
| `Theme` | Quick theme / JSON / font accents (full Style Editor is Preferences) |
| `Preferences` | File → Preferences… |

#### Example Usage

```cpp
void MyApp::setup() {
    if (auto* mui = dynamic_cast<rigkit::Mui*>(m_engine->getUiManager())) {
        // Sketch apps: every host panel + first-run dock layout
        mui->addAllHostPanels();

        // Or pick a subset (tool apps):
        // mui->addHostPanel(rigkit::HostPanel::Properties);
        // mui->addHostPanels({rigkit::HostPanel::Properties, rigkit::HostPanel::Log});
    }
}
```

## Creating Custom Windows

### Basic Custom Window

```cpp
class MyApp : public rigkit::IApp {
public:
    // Custom window as nested class
    class MyCustomWindow : public rigkit::IWindow {
    public:
        MyCustomWindow() : IWindow("My Window", ImGuiWindowFlags_None) {}

        void renderContents() override {
            ImGui::Text("Hello from my custom window!");
            if (ImGui::Button("Click me!")) {
                // Handle button click
            }
        }
    };

    void setup() override {
        auto windowManager = getUIManager()->getWindowManager();

        // Create and show custom window
        auto myWindow = std::make_shared<MyCustomWindow>();
        windowManager->createWindow(myWindow);
        windowManager->showWindow("My Window");
    }
};
```

### Advanced Custom Window

```cpp
class MyApp : public rigkit::IApp {
public:
    // Advanced custom window as nested class
    class AdvancedWindow : public rigkit::IWindow {
    private:
        float m_value = 0.0f;
        std::string m_text;

    public:
        AdvancedWindow() : IWindow("Advanced Window", ImGuiWindowFlags_MenuBar) {}

        void renderContents() override {
            // Menu bar
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Save")) {
                        // Handle save
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            // Window contents
            ImGui::SliderFloat("Value", &m_value, 0.0f, 100.0f);
            ImGui::InputText("Text", &m_text);

            if (ImGui::Button("Reset")) {
                m_value = 0.0f;
                m_text.clear();
            }
        }
    };
};
```

## Window Management

### Standard Windows
Standard windows are automatically created by the pack and can be controlled via visibility:

```cpp
// In your IApp::setup() method
void MyApp::setup() {
    // Show/hide standard windows
    setWindowVisibility("LogWindow", true);
    setWindowVisibility("DebugPanel", true);
    setWindowVisibility("PropertiesWindow", false);
}
```

### Creating Custom Windows

```cpp
// In your IApp::setup() method
void MyApp::setup() {
    auto windowManager = getUIManager()->getWindowManager();

    // Create and show a custom window
    auto window = std::make_shared<MyCustomWindow>();
    windowManager->createWindow(window);
    windowManager->showWindow("My Window");
}
```

### Window Visibility

```cpp
// Show/hide specific windows
uiManager->setWindowVisibility("My Window", true);
uiManager->setWindowVisibility("Debug Panel", false);

// Show/hide all windows
uiManager->setWindowVisibilityAll(true);
```

### Window Access

```cpp
// Get window by name
auto window = windowManager->getWindow<MyWindow>("My Window");

// Get all window names
auto windowNames = uiManager->getAllWindowNames();
```

## Theme Management

### Built-in Themes

```cpp
// Set theme
uiManager->setImGuiTheme(rigkit::ImGuiTheme::Dark);
uiManager->setImGuiTheme(rigkit::ImGuiTheme::Light);
```

### Custom Themes

```cpp
// Save current theme
uiManager->saveCurrentTheme("themes/my_theme.json");

// Load theme
uiManager->loadTheme("themes/my_theme.json");
```

## Workspaces

Named dock layouts, Photoshop-style: snapshot the current layout **and window visibility** under a name, switch between snapshots from **App → Workspace**. Each workspace is a `<name>.ini` under `AppPaths::getWorkspacesDir()` (`data/user/workspaces/`) — ImGui dock data plus a `[RigVisibility][Windows]` section. The live session keeps autosaving to `imgui.ini` there.

On startup, rigImGui loads the last used named workspace (settings key `workspace`) when that file exists; otherwise it loads **`Standard`**. If `Standard.ini` is missing, a one-shot dock builder (host `setFirstRunHostDockLayout` and/or the app’s `setDockLayoutBuilder`) seeds it from the first settled split, then that file becomes the durable default — not a hardcoded layout forever.

```cpp
// Snapshot the current dock layout + which IWindows are visible
uiManager->saveWorkspace("plotting");

// Apply a saved layout (deferred to the next frame boundary)
uiManager->loadWorkspace("editing");

// Enumerate / remove (Standard cannot be deleted via the API/menu)
auto names = uiManager->workspaceNames();
uiManager->deleteWorkspace("old_layout");
```

Menu: **App → Workspace** lists saved workspaces (checkmark on the active one), **Save Workspace As...** prompts for a name, **Delete Workspace** confirms before removing (Standard is omitted). Loading a workspace that includes `[RigVisibility]` hides every registered window, then shows only those marked visible — panels not in the snapshot stay hidden.

Legacy `.ini` files without `[RigVisibility]` still restore docks; visibility is left unchanged until the next save writes the section.

## Chrome rules

Host chrome (dock layout, notifications, prefs-driven spacing) stays inside the main viewport work rect and scales with DPI.

- Prefer relative layout (`WorkSize` fractions, `GetContentRegionAvail`, `AlwaysAutoResize`) so you never multiply by scale.
- When a design (1x) size is required, convert with `uiPx` / `uiSize` from `UiDpi.h` (reads `style.FontScaleDpi` after `Mui::applyDpiStyle`). Do not store a private `m_dpiScale` or push `setDpiScale` into child widgets.
- Keep chrome inside `GetMainViewport()->WorkPos` / `WorkSize`: `SetNextWindowPos` with a pivot plus `SetNextWindowSizeConstraints`, never a corrective `SetWindowPos` after `Begin`. Absolute sizes that must stay on-screen can use `uiClampToWork` / `uiWindowSize`.
- Default layout is the `Standard` workspace (`Standard.ini`), seeded once from a dock builder when missing; startup loads last used or `Standard`.
- Chrome preferences live on `UiPrefs` (section `rigImGui.ui`) and must be read where they apply.
- No emoji in UI text; IconFont glyphs are fine.

## Notification System

### Notifications

Duration defaults to Interface **Notification Seconds** when omitted (or negative). Width and placement follow [Chrome rules](#chrome-rules) and **Notification Width**.

```cpp
// Show different types of notifications
uiManager->showNotification("Operation completed successfully!", 
                          rigkit::NotificationType::Success);
uiManager->showNotification("Warning: Low memory", 
                          rigkit::NotificationType::Warning);
uiManager->showNotification("Error: Failed to save", 
                          rigkit::NotificationType::Error);
uiManager->showNotification("Info: New update available", 
                          rigkit::NotificationType::Info);
```

### Modal Dialogs

```cpp
// Show modal with callback
uiManager->showModal("Confirm Delete", 
                    "Are you sure you want to delete this item?",
                    rigkit::NotificationType::Warning,
                    []() { /* Handle OK */ });

// Show simple modal
uiManager->showModal("Information", 
                    "This is an informational message.",
                    rigkit::NotificationType::Info);
```

### Event System Integration

`Mui` reaches the bus through the engine it is attached to — nothing is pushed in:

```cpp
auto& events = engine->getECSManager()->getEventSystem();
```

Apps register their own actions in `IApp::registerUIActions(SEvent&)`.

[API/docs](https://rigkid.github.io/rigImGui/)
