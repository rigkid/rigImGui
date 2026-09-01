# Read pack name
file(READ "${CMAKE_CURRENT_LIST_DIR}/pack.json" PACK_JSON)
string(JSON PACK_NAME GET "${PACK_JSON}" name)

# Hosts that already link PDF-Writer's bundled FreeType (Relayout) must not
# also pull Freetype::Freetype — two FT copies collide at link (FT_Done_Face).
option(RIGIMGUI_SYSTEM_FREETYPE
	"Link system FreeType (OFF: headers only; symbols from PDF-Writer bundled FT)"
	ON)

# Scene / Layers / handles need component headers and PrimitiveBounds.
set(_rigComponent_dir "${CMAKE_CURRENT_LIST_DIR}/../rigComponent")
set(_rigComponent_src "${_rigComponent_dir}/src")
if(NOT EXISTS "${_rigComponent_src}/CRelationship.h")
	message(FATAL_ERROR "rigImGui host shell requires packs/rigComponent next to rigImGui")
endif()
# OrthoViewWindow presents through SMeshPresent3D. Optional sibling — 2D apps
# omit it; only TUs that call openOrthoView need the window at link.
set(_rigRender3D_src "")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../rigRender3D/src/SMeshPresent3D.h")
	set(_rigRender3D_src "${CMAKE_CURRENT_LIST_DIR}/../rigRender3D/src")
elseif(DEFINED RIGKIT_ROOT AND EXISTS "${RIGKIT_ROOT}/packs/rigRender3D/src/SMeshPresent3D.h")
	set(_rigRender3D_src "${RIGKIT_ROOT}/packs/rigRender3D/src")
endif()
if(NOT TARGET rigComponent)
	add_subdirectory("${_rigComponent_dir}" "${CMAKE_BINARY_DIR}/packs/rigComponent")
endif()

# stb_image_write for File to Export PNG (from host GLFW deps).
set(_stb_write_dir "")
if(DEFINED RIGKIT_ROOT AND EXISTS "${RIGKIT_ROOT}/third_party/glfw/deps/stb_image_write.h")
	set(_stb_write_dir "${RIGKIT_ROOT}/third_party/glfw/deps")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../third_party/glfw/deps/stb_image_write.h")
	set(_stb_write_dir "${CMAKE_CURRENT_LIST_DIR}/../../third_party/glfw/deps")
endif()
if(_stb_write_dir STREQUAL "")
	message(FATAL_ERROR "rigImGui Export PNG needs third_party/glfw/deps/stb_image_write.h")
endif()

# rigImGui host shell - themes/fonts via ImGuiStyleKit
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
    src/ImGuiExpr.cpp
    src/ImGuiExpr.h
    src/PreferencesPanel.cpp
    src/PreferencesPanel.h
    src/ThemePanel.cpp
    src/ThemePanel.h
    src/ThemePicker.cpp
    src/ThemePicker.h
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
    src/AnchorPad.cpp
    src/AnchorPad.h
    src/EntityPick.cpp
    src/EntityPick.h
    src/ReorderDragDrop.cpp
    src/ReorderDragDrop.h
    src/ChainEditor.cpp
    src/ChainEditor.h
)

add_library(${PACK_NAME} STATIC ${PACK_SOURCES})
target_link_libraries(${PACK_NAME} PUBLIC rigkit glfw rigComponent)
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
if(_rigRender3D_src)
	target_sources(${PACK_NAME} PRIVATE src/OrthoViewWindow.cpp src/OrthoViewWindow.h)
	target_include_directories(${PACK_NAME} PUBLIC "${_rigRender3D_src}")
endif()

file(GLOB DEP_SOURCES
    "third_party/imgui/*.cpp"
    "third_party/imgui/backends/imgui_impl_glfw.cpp"
    "third_party/imgui/backends/imgui_impl_opengl3.cpp"
)
target_sources(${PACK_NAME} PRIVATE ${DEP_SOURCES})

find_package(Freetype QUIET)
if(NOT Freetype_FOUND)
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(FREETYPE QUIET IMPORTED_TARGET freetype2)
	endif()
endif()
if(Freetype_FOUND OR TARGET PkgConfig::FREETYPE)
	target_sources(${PACK_NAME} PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/third_party/imgui/misc/freetype/imgui_freetype.cpp")
	target_include_directories(${PACK_NAME} PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/third_party/imgui/misc/freetype")
	target_compile_definitions(${PACK_NAME} PUBLIC IMGUI_ENABLE_FREETYPE)
	if(RIGIMGUI_SYSTEM_FREETYPE)
		if(Freetype_FOUND)
			target_link_libraries(${PACK_NAME} PUBLIC Freetype::Freetype)
		else()
			target_link_libraries(${PACK_NAME} PUBLIC PkgConfig::FREETYPE)
		endif()
		message(STATUS "Configured pack: ${PACK_NAME} (rigImGui host shell, FreeType wght)")
	else()
		# Compile imgui_freetype against system headers; do not link the DLL
		# import lib. Relayout resolves FT from PDF-Writer's bundled static lib.
		if(Freetype_FOUND)
			get_target_property(_ft_inc Freetype::Freetype INTERFACE_INCLUDE_DIRECTORIES)
			if(_ft_inc)
				target_include_directories(${PACK_NAME} PRIVATE ${_ft_inc})
			endif()
		elseif(TARGET PkgConfig::FREETYPE)
			target_include_directories(${PACK_NAME} PRIVATE ${FREETYPE_INCLUDE_DIRS})
		endif()
		message(STATUS "Configured pack: ${PACK_NAME} (rigImGui host shell, FreeType headers only)")
	endif()
else()
	message(STATUS "Configured pack: ${PACK_NAME} (rigImGui host shell, no FreeType)")
endif()
