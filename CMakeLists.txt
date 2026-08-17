# Read pack name
file(READ "${CMAKE_CURRENT_LIST_DIR}/pack.json" PACK_JSON)
string(JSON PACK_NAME GET "${PACK_JSON}" name)

# Scene / Layers / handles need component headers (data only).
set(_rigComponent_src "${CMAKE_CURRENT_LIST_DIR}/../rigComponent/src")
if(NOT EXISTS "${_rigComponent_src}/CRelationship.h")
	message(FATAL_ERROR "rigImGui host shell requires packs/rigComponent next to rigImGui")
endif()

# stb_image_write for File→Export PNG (from host GLFW deps).
set(_stb_write_dir "")
if(DEFINED RIGKIT_ROOT AND EXISTS "${RIGKIT_ROOT}/third_party/glfw/deps/stb_image_write.h")
	set(_stb_write_dir "${RIGKIT_ROOT}/third_party/glfw/deps")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../third_party/glfw/deps/stb_image_write.h")
	set(_stb_write_dir "${CMAKE_CURRENT_LIST_DIR}/../../third_party/glfw/deps")
endif()
if(_stb_write_dir STREQUAL "")
	message(FATAL_ERROR "rigImGui Export PNG needs third_party/glfw/deps/stb_image_write.h")
endif()

# rigImGui host shell — themes/fonts via ImGuiStyleKit
set(PACK_SOURCES
    src/rigImGui.cpp
    src/rigImGui.h
    src/Mui.cpp
    src/Mui.h
    src/ImGuiStyleKit.cpp
    src/ImGuiStyleKit.h
    src/MWindow.cpp
    src/MWindow.h
    src/IWindow.cpp
    src/IWindow.h
    src/HostMenuBar.cpp
    src/HostMenuBar.h
    src/LogWindow.cpp
    src/LogWindow.h
    src/WindowManagerPanel.cpp
    src/WindowManagerPanel.h
    src/ImGuiWindow.cpp
    src/ImGuiWindow.h
    src/DebugPanel.cpp
    src/DebugPanel.h
    src/PropertiesWindow.cpp
    src/PropertiesWindow.h
    src/PropEditors.cpp
    src/PropEditors.h
    src/PreferencesPanel.cpp
    src/PreferencesPanel.h
    src/ThemePanel.cpp
    src/ThemePanel.h
    src/TtfKern.cpp
    src/TtfKern.h
    src/ShortcutManager.cpp
    src/ShortcutManager.h
    src/ShortcutsPanel.cpp
    src/ShortcutsPanel.h
    src/StatusBar.cpp
    src/StatusBar.h
    src/SceneWindow.cpp
    src/SceneWindow.h
    src/LayersWindow.cpp
    src/LayersWindow.h
    src/ViewportWindow.cpp
    src/ViewportWindow.h
    src/FileDialogs.cpp
    src/FileDialogs.h
    src/UiDpi.h
    src/FontPicker.cpp
    src/FontPicker.h
    src/PresetBar.cpp
    src/PresetBar.h
    src/ExportPng.cpp
    src/ExportPng.h
    src/Rulers.cpp
    src/Rulers.h
    src/Handle2D.cpp
    src/Handle2D.h
    src/EntityPick.cpp
    src/EntityPick.h
    src/ReorderDragDrop.cpp
    src/ReorderDragDrop.h
    src/ChainEditor.cpp
    src/ChainEditor.h
)

add_library(${PACK_NAME} STATIC ${PACK_SOURCES})
target_link_libraries(${PACK_NAME} PUBLIC rigkit glfw)
if(WIN32)
	target_link_libraries(${PACK_NAME} PRIVATE Psapi)
endif()
target_include_directories(${PACK_NAME} PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/src"
    "${CMAKE_CURRENT_LIST_DIR}/third_party/imgui"
    "${CMAKE_CURRENT_LIST_DIR}/third_party/imgui/backends"
    "${CMAKE_CURRENT_LIST_DIR}/third_party/IconFontCppHeaders"
    "${CMAKE_CURRENT_LIST_DIR}/third_party/imgui-filebrowser"
    "${CMAKE_CURRENT_LIST_DIR}/third_party/ImGuizmo"
    "${CMAKE_CURRENT_LIST_DIR}/fonts"
    "${_rigComponent_src}"
    "${_stb_write_dir}"
)

file(GLOB DEP_SOURCES
    "third_party/imgui/*.cpp"
    "third_party/imgui/backends/imgui_impl_glfw.cpp"
    "third_party/imgui/backends/imgui_impl_opengl3.cpp"
)
target_sources(${PACK_NAME} PRIVATE ${DEP_SOURCES})

message(STATUS "Configured pack: ${PACK_NAME} (rigImGui host shell)")
