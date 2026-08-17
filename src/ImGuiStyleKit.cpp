#include "ImGuiStyleKit.h"

#include "imgui.h"

#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <vector>

// Embedded body font (may be a stub — see fonts/fontRobotoRegular.h).
#include "fontRobotoRegular.h"

#if __has_include("IconsFontAwesome5.h")
#include "IconsFontAwesome5.h"
#define RIGIMGUI_HAS_ICON_HEADERS 1
#else
#define RIGIMGUI_HAS_ICON_HEADERS 0
#endif

namespace rigkit {
namespace ImGuiStyleKit {
namespace {

void applyBaseMetrics() {
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(10.0f, 10.0f);
	style.FramePadding = ImVec2(8.0f, 5.0f);
	style.CellPadding = ImVec2(6.0f, 3.0f);
	style.ItemSpacing = ImVec2(8.0f, 6.0f);
	style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
	style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
	style.IndentSpacing = 18.0f;
	style.ScrollbarSize = 14.0f;
	style.GrabMinSize = 10.0f;

	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	// Frame border keeps InputText / Combo visible when FrameBg is close to
	// WindowBg or PopupBg (file dialogs, dark theme).
	style.FrameBorderSize = 1.0f;
	style.TabBorderSize = 0.0f;

	style.WindowRounding = 5.0f;
	style.ChildRounding = 4.0f;
	style.FrameRounding = 3.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 6.0f;
	style.GrabRounding = 3.0f;
	style.TabRounding = 3.0f;

	style.WindowTitleAlign = ImVec2(0.02f, 0.5f);
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
	style.DisplaySafeAreaPadding = ImVec2(4.0f, 4.0f);
}

void applyDarkColors() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* c = style.Colors;

	const ImVec4 bg(0.12f, 0.13f, 0.15f, 1.00f);
	const ImVec4 bg2(0.16f, 0.17f, 0.20f, 1.00f);
	const ImVec4 bg3(0.20f, 0.22f, 0.26f, 1.00f);
	const ImVec4 fg(0.92f, 0.93f, 0.94f, 1.00f);
	const ImVec4 muted(0.55f, 0.58f, 0.62f, 1.00f);
	const ImVec4 accent(0.30f, 0.72f, 0.68f, 1.00f);
	const ImVec4 accentHi(0.40f, 0.82f, 0.76f, 1.00f);
	const ImVec4 accentDim(0.22f, 0.48f, 0.46f, 1.00f);

	c[ImGuiCol_Text] = fg;
	c[ImGuiCol_TextDisabled] = muted;
	c[ImGuiCol_WindowBg] = bg;
	c[ImGuiCol_ChildBg] = ImVec4(bg.x, bg.y, bg.z, 0.00f);
	c[ImGuiCol_PopupBg] = bg;
	c[ImGuiCol_Border] = ImVec4(0.28f, 0.30f, 0.34f, 0.70f);
	c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	c[ImGuiCol_FrameBg] = bg2;
	c[ImGuiCol_FrameBgHovered] = bg3;
	c[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.28f, 0.32f, 1.00f);
	c[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.11f, 1.00f);
	c[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.18f, 0.18f, 1.00f);
	c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.09f, 0.10f, 0.11f, 0.75f);
	c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
	c[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.11f, 0.12f, 0.60f);
	c[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.38f, 0.42f, 1.00f);
	c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.42f, 0.46f, 0.50f, 1.00f);
	c[ImGuiCol_ScrollbarGrabActive] = accent;
	c[ImGuiCol_CheckMark] = accent;
	c[ImGuiCol_SliderGrab] = accentDim;
	c[ImGuiCol_SliderGrabActive] = accent;
	c[ImGuiCol_Button] = bg3;
	c[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.40f, 0.40f, 1.00f);
	c[ImGuiCol_ButtonActive] = accentDim;
	c[ImGuiCol_Header] = ImVec4(0.22f, 0.32f, 0.32f, 0.70f);
	c[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.42f, 0.40f, 0.85f);
	c[ImGuiCol_HeaderActive] = accentDim;
	c[ImGuiCol_Separator] = ImVec4(0.30f, 0.32f, 0.36f, 0.60f);
	c[ImGuiCol_SeparatorHovered] = accent;
	c[ImGuiCol_SeparatorActive] = accentHi;
	c[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.32f, 0.36f, 0.40f);
	c[ImGuiCol_ResizeGripHovered] = accent;
	c[ImGuiCol_ResizeGripActive] = accentHi;
	c[ImGuiCol_Tab] = bg2;
	c[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.42f, 0.40f, 0.90f);
	c[ImGuiCol_TabActive] = ImVec4(0.22f, 0.34f, 0.33f, 1.00f);
	c[ImGuiCol_TabUnfocused] = bg;
	c[ImGuiCol_TabUnfocusedActive] = bg2;
	c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
	c[ImGuiCol_DockingEmptyBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
	c[ImGuiCol_PlotLines] = accentHi;
	c[ImGuiCol_PlotLinesHovered] = accent;
	c[ImGuiCol_PlotHistogram] = accentDim;
	c[ImGuiCol_PlotHistogramHovered] = accent;
	c[ImGuiCol_TableHeaderBg] = bg2;
	c[ImGuiCol_TableBorderStrong] = ImVec4(0.28f, 0.30f, 0.34f, 1.00f);
	c[ImGuiCol_TableBorderLight] = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
	c[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	c[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
	c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
	c[ImGuiCol_DragDropTarget] = accentHi;
	c[ImGuiCol_NavHighlight] = accent;
	c[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	c[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
	c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.55f);
}

void applyLightColors() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* c = style.Colors;

	const ImVec4 bg(0.94f, 0.95f, 0.96f, 1.00f);
	const ImVec4 panel(1.00f, 1.00f, 1.00f, 1.00f);
	const ImVec4 text(0.14f, 0.16f, 0.18f, 1.00f);
	const ImVec4 muted(0.48f, 0.50f, 0.54f, 1.00f);
	const ImVec4 accent(0.12f, 0.52f, 0.50f, 1.00f);
	const ImVec4 accentHi(0.16f, 0.62f, 0.58f, 1.00f);

	c[ImGuiCol_Text] = text;
	c[ImGuiCol_TextDisabled] = muted;
	c[ImGuiCol_WindowBg] = panel;
	c[ImGuiCol_ChildBg] = ImVec4(bg.x, bg.y, bg.z, 0.00f);
	c[ImGuiCol_PopupBg] = panel;
	c[ImGuiCol_Border] = ImVec4(0.78f, 0.80f, 0.84f, 1.00f);
	c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	c[ImGuiCol_FrameBg] = bg;
	c[ImGuiCol_FrameBgHovered] = ImVec4(0.88f, 0.92f, 0.92f, 1.00f);
	c[ImGuiCol_FrameBgActive] = ImVec4(0.82f, 0.90f, 0.89f, 1.00f);
	c[ImGuiCol_TitleBg] = ImVec4(0.90f, 0.92f, 0.93f, 1.00f);
	c[ImGuiCol_TitleBgActive] = ImVec4(0.78f, 0.88f, 0.86f, 1.00f);
	c[ImGuiCol_TitleBgCollapsed] = bg;
	c[ImGuiCol_MenuBarBg] = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
	c[ImGuiCol_ScrollbarBg] = bg;
	c[ImGuiCol_ScrollbarGrab] = ImVec4(0.70f, 0.74f, 0.78f, 1.00f);
	c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.66f, 0.70f, 1.00f);
	c[ImGuiCol_ScrollbarGrabActive] = accent;
	c[ImGuiCol_CheckMark] = accent;
	c[ImGuiCol_SliderGrab] = accent;
	c[ImGuiCol_SliderGrabActive] = accentHi;
	c[ImGuiCol_Button] = ImVec4(0.88f, 0.91f, 0.92f, 1.00f);
	c[ImGuiCol_ButtonHovered] = ImVec4(0.78f, 0.90f, 0.88f, 1.00f);
	c[ImGuiCol_ButtonActive] = accentHi;
	c[ImGuiCol_Header] = ImVec4(0.82f, 0.90f, 0.89f, 1.00f);
	c[ImGuiCol_HeaderHovered] = ImVec4(0.74f, 0.88f, 0.86f, 1.00f);
	c[ImGuiCol_HeaderActive] = accent;
	c[ImGuiCol_Separator] = ImVec4(0.78f, 0.80f, 0.84f, 1.00f);
	c[ImGuiCol_SeparatorHovered] = accent;
	c[ImGuiCol_SeparatorActive] = accentHi;
	c[ImGuiCol_ResizeGrip] = ImVec4(0.70f, 0.74f, 0.78f, 0.40f);
	c[ImGuiCol_ResizeGripHovered] = accent;
	c[ImGuiCol_ResizeGripActive] = accentHi;
	c[ImGuiCol_Tab] = bg;
	c[ImGuiCol_TabHovered] = ImVec4(0.74f, 0.88f, 0.86f, 1.00f);
	c[ImGuiCol_TabActive] = panel;
	c[ImGuiCol_TabUnfocused] = bg;
	c[ImGuiCol_TabUnfocusedActive] = panel;
	c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
	c[ImGuiCol_DockingEmptyBg] = bg;
	c[ImGuiCol_PlotLines] = accent;
	c[ImGuiCol_PlotHistogram] = accentHi;
	c[ImGuiCol_TableHeaderBg] = bg;
	c[ImGuiCol_TableBorderStrong] = ImVec4(0.78f, 0.80f, 0.84f, 1.00f);
	c[ImGuiCol_TableBorderLight] = ImVec4(0.86f, 0.88f, 0.90f, 1.00f);
	c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.28f);
	c[ImGuiCol_DragDropTarget] = accentHi;
	c[ImGuiCol_NavHighlight] = accent;
	c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.22f, 0.35f);
}

void applyDraculaColors() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* c = style.Colors;
	const ImVec4 bg(0.16f, 0.16f, 0.21f, 1.00f);
	const ImVec4 bg2(0.20f, 0.20f, 0.27f, 1.00f);
	const ImVec4 fg(0.97f, 0.97f, 0.95f, 1.00f);
	const ImVec4 purple(0.74f, 0.58f, 0.98f, 1.00f);
	const ImVec4 pink(1.00f, 0.47f, 0.78f, 1.00f);
	const ImVec4 cyan(0.55f, 0.90f, 0.94f, 1.00f);

	c[ImGuiCol_Text] = fg;
	c[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);
	c[ImGuiCol_WindowBg] = bg;
	c[ImGuiCol_ChildBg] = ImVec4(bg.x, bg.y, bg.z, 0.00f);
	c[ImGuiCol_PopupBg] = bg;
	c[ImGuiCol_Border] = ImVec4(0.35f, 0.36f, 0.45f, 0.60f);
	c[ImGuiCol_FrameBg] = bg2;
	c[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.40f, 1.00f);
	c[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.35f, 0.48f, 1.00f);
	c[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
	c[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.16f, 0.28f, 1.00f);
	c[ImGuiCol_TitleBgCollapsed] = bg;
	c[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
	c[ImGuiCol_ScrollbarBg] = bg;
	c[ImGuiCol_ScrollbarGrab] = ImVec4(0.40f, 0.40f, 0.50f, 1.00f);
	c[ImGuiCol_CheckMark] = purple;
	c[ImGuiCol_SliderGrab] = purple;
	c[ImGuiCol_SliderGrabActive] = pink;
	c[ImGuiCol_Button] = ImVec4(0.35f, 0.28f, 0.55f, 1.00f);
	c[ImGuiCol_ButtonHovered] = ImVec4(0.45f, 0.35f, 0.70f, 1.00f);
	c[ImGuiCol_ButtonActive] = pink;
	c[ImGuiCol_Header] = ImVec4(0.35f, 0.28f, 0.55f, 0.70f);
	c[ImGuiCol_HeaderHovered] = ImVec4(0.45f, 0.35f, 0.70f, 0.85f);
	c[ImGuiCol_HeaderActive] = purple;
	c[ImGuiCol_Separator] = ImVec4(0.40f, 0.40f, 0.50f, 0.60f);
	c[ImGuiCol_Tab] = bg2;
	c[ImGuiCol_TabHovered] = ImVec4(0.45f, 0.35f, 0.70f, 0.85f);
	c[ImGuiCol_TabActive] = ImVec4(0.35f, 0.28f, 0.55f, 1.00f);
	c[ImGuiCol_TabUnfocused] = bg;
	c[ImGuiCol_TabUnfocusedActive] = bg2;
	c[ImGuiCol_DockingPreview] = ImVec4(purple.x, purple.y, purple.z, 0.40f);
	c[ImGuiCol_PlotLines] = cyan;
	c[ImGuiCol_PlotHistogram] = purple;
	c[ImGuiCol_TextSelectedBg] = ImVec4(0.45f, 0.35f, 0.70f, 0.45f);
	c[ImGuiCol_NavHighlight] = cyan;
}

void applyCorporateColors() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* c = style.Colors;
	const ImVec4 bg(0.93f, 0.94f, 0.96f, 1.00f);
	const ImVec4 panel(1.00f, 1.00f, 1.00f, 1.00f);
	const ImVec4 text(0.12f, 0.14f, 0.18f, 1.00f);
	const ImVec4 accent(0.12f, 0.35f, 0.65f, 1.00f);
	const ImVec4 accentHi(0.18f, 0.45f, 0.80f, 1.00f);

	c[ImGuiCol_Text] = text;
	c[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.52f, 1.00f);
	c[ImGuiCol_WindowBg] = panel;
	c[ImGuiCol_ChildBg] = ImVec4(bg.x, bg.y, bg.z, 0.00f);
	c[ImGuiCol_PopupBg] = panel;
	c[ImGuiCol_Border] = ImVec4(0.75f, 0.78f, 0.82f, 1.00f);
	c[ImGuiCol_FrameBg] = bg;
	c[ImGuiCol_FrameBgHovered] = ImVec4(0.88f, 0.90f, 0.94f, 1.00f);
	c[ImGuiCol_FrameBgActive] = ImVec4(0.82f, 0.86f, 0.92f, 1.00f);
	c[ImGuiCol_TitleBg] = ImVec4(0.88f, 0.90f, 0.93f, 1.00f);
	c[ImGuiCol_TitleBgActive] = accent;
	c[ImGuiCol_TitleBgCollapsed] = bg;
	c[ImGuiCol_MenuBarBg] = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
	c[ImGuiCol_ScrollbarBg] = bg;
	c[ImGuiCol_ScrollbarGrab] = ImVec4(0.65f, 0.70f, 0.78f, 1.00f);
	c[ImGuiCol_CheckMark] = accent;
	c[ImGuiCol_SliderGrab] = accent;
	c[ImGuiCol_SliderGrabActive] = accentHi;
	c[ImGuiCol_Button] = ImVec4(0.82f, 0.86f, 0.92f, 1.00f);
	c[ImGuiCol_ButtonHovered] = ImVec4(0.72f, 0.80f, 0.92f, 1.00f);
	c[ImGuiCol_ButtonActive] = accentHi;
	c[ImGuiCol_Header] = ImVec4(0.78f, 0.84f, 0.92f, 1.00f);
	c[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.78f, 0.90f, 1.00f);
	c[ImGuiCol_HeaderActive] = accent;
	c[ImGuiCol_Separator] = ImVec4(0.75f, 0.78f, 0.82f, 1.00f);
	c[ImGuiCol_Tab] = bg;
	c[ImGuiCol_TabHovered] = ImVec4(0.72f, 0.80f, 0.92f, 1.00f);
	c[ImGuiCol_TabActive] = panel;
	c[ImGuiCol_TabUnfocused] = bg;
	c[ImGuiCol_TabUnfocusedActive] = panel;
	c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
	c[ImGuiCol_TextSelectedBg] = ImVec4(0.18f, 0.45f, 0.80f, 0.35f);
	c[ImGuiCol_NavHighlight] = accent;
	c[ImGuiCol_TitleBgActive] = accent;
}

std::filesystem::path findFontFile(const std::string& fontsSearchDir, const char* fileName) {
	namespace fs = std::filesystem;
	std::vector<fs::path> candidates;
	if (!fontsSearchDir.empty()) {
		candidates.emplace_back(fs::path(fontsSearchDir) / fileName);
	}
	candidates.emplace_back(fs::path("packs/rigImGui/fonts") / fileName);
	candidates.emplace_back(fs::path("assets/fonts") / fileName);
	for (const auto& p : candidates) {
		std::error_code ec;
		if (fs::is_regular_file(p, ec)) {
			return fs::weakly_canonical(p, ec);
		}
	}
	return {};
}

} // namespace

void applyStyleExtras(ImGuiTheme theme) {
	applyBaseMetrics();
	ImGuiStyle& style = ImGui::GetStyle();

	switch (theme) {
	case ImGuiTheme::Corporate:
		style.WindowRounding = 2.0f;
		style.ChildRounding = 2.0f;
		style.FrameRounding = 2.0f;
		style.PopupRounding = 2.0f;
		style.TabRounding = 2.0f;
		style.FrameBorderSize = 1.0f;
		style.WindowPadding = ImVec2(10.0f, 10.0f);
		style.FramePadding = ImVec2(8.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 5.0f);
		break;
	case ImGuiTheme::Dracula:
		style.WindowRounding = 6.0f;
		style.ChildRounding = 5.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 5.0f;
		style.TabRounding = 4.0f;
		style.WindowPadding = ImVec2(12.0f, 12.0f);
		style.FramePadding = ImVec2(9.0f, 5.0f);
		break;
	case ImGuiTheme::Light:
		style.FrameBorderSize = 1.0f;
		style.WindowRounding = 4.0f;
		break;
	case ImGuiTheme::Classic:
		style.WindowRounding = 0.0f;
		style.ChildRounding = 0.0f;
		style.FrameRounding = 0.0f;
		style.PopupRounding = 0.0f;
		style.ScrollbarRounding = 0.0f;
		style.GrabRounding = 0.0f;
		style.TabRounding = 0.0f;
		style.FrameBorderSize = 1.0f;
		break;
	case ImGuiTheme::Dark:
	default:
		break;
	}
}

void applyTheme(ImGuiTheme theme) {
	switch (theme) {
	case ImGuiTheme::Light:
		ImGui::StyleColorsLight();
		applyLightColors();
		break;
	case ImGuiTheme::Classic:
		ImGui::StyleColorsClassic();
		break;
	case ImGuiTheme::Corporate:
		ImGui::StyleColorsLight();
		applyCorporateColors();
		break;
	case ImGuiTheme::Dracula:
		ImGui::StyleColorsDark();
		applyDraculaColors();
		break;
	case ImGuiTheme::Dark:
	default:
		ImGui::StyleColorsDark();
		applyDarkColors();
		break;
	}
	applyStyleExtras(theme);
}

json styleToJson(const ImGuiStyle& style, int baseTheme) {
	json j;
	j["version"] = 1;
	j["baseTheme"] = baseTheme;

	json colors = json::object();
	for (int i = 0; i < ImGuiCol_COUNT; ++i) {
		const ImVec4& c = style.Colors[i];
		colors[ImGui::GetStyleColorName(i)] = {c.x, c.y, c.z, c.w};
	}
	j["colors"] = std::move(colors);

	// Full ShowStyleEditor surface (portable JSON, not a binary dump).
	json s;
	s["Alpha"] = style.Alpha;
	s["DisabledAlpha"] = style.DisabledAlpha;
	s["WindowPadding"] = {style.WindowPadding.x, style.WindowPadding.y};
	s["WindowRounding"] = style.WindowRounding;
	s["WindowBorderSize"] = style.WindowBorderSize;
	s["WindowBorderHoverPadding"] = style.WindowBorderHoverPadding;
	s["WindowMinSize"] = {style.WindowMinSize.x, style.WindowMinSize.y};
	s["WindowTitleAlign"] = {style.WindowTitleAlign.x, style.WindowTitleAlign.y};
	s["WindowMenuButtonPosition"] = static_cast<int>(style.WindowMenuButtonPosition);
	s["ChildRounding"] = style.ChildRounding;
	s["ChildBorderSize"] = style.ChildBorderSize;
	s["PopupRounding"] = style.PopupRounding;
	s["PopupBorderSize"] = style.PopupBorderSize;
	s["FramePadding"] = {style.FramePadding.x, style.FramePadding.y};
	s["FrameRounding"] = style.FrameRounding;
	s["FrameBorderSize"] = style.FrameBorderSize;
	s["ItemSpacing"] = {style.ItemSpacing.x, style.ItemSpacing.y};
	s["ItemInnerSpacing"] = {style.ItemInnerSpacing.x, style.ItemInnerSpacing.y};
	s["CellPadding"] = {style.CellPadding.x, style.CellPadding.y};
	s["TouchExtraPadding"] = {style.TouchExtraPadding.x, style.TouchExtraPadding.y};
	s["IndentSpacing"] = style.IndentSpacing;
	s["ColumnsMinSpacing"] = style.ColumnsMinSpacing;
	s["ScrollbarSize"] = style.ScrollbarSize;
	s["ScrollbarRounding"] = style.ScrollbarRounding;
	s["GrabMinSize"] = style.GrabMinSize;
	s["GrabRounding"] = style.GrabRounding;
	s["LogSliderDeadzone"] = style.LogSliderDeadzone;
	s["ImageBorderSize"] = style.ImageBorderSize;
	s["TabRounding"] = style.TabRounding;
	s["TabBorderSize"] = style.TabBorderSize;
	s["TabCloseButtonMinWidthSelected"] = style.TabCloseButtonMinWidthSelected;
	s["TabCloseButtonMinWidthUnselected"] = style.TabCloseButtonMinWidthUnselected;
	s["TabBarBorderSize"] = style.TabBarBorderSize;
	s["TabBarOverlineSize"] = style.TabBarOverlineSize;
	s["TableAngledHeadersAngle"] = style.TableAngledHeadersAngle;
	s["TableAngledHeadersTextAlign"] = {style.TableAngledHeadersTextAlign.x,
										style.TableAngledHeadersTextAlign.y};
	s["TreeLinesFlags"] = static_cast<int>(style.TreeLinesFlags);
	s["TreeLinesSize"] = style.TreeLinesSize;
	s["TreeLinesRounding"] = style.TreeLinesRounding;
	s["ColorButtonPosition"] = static_cast<int>(style.ColorButtonPosition);
	s["ButtonTextAlign"] = {style.ButtonTextAlign.x, style.ButtonTextAlign.y};
	s["SelectableTextAlign"] = {style.SelectableTextAlign.x, style.SelectableTextAlign.y};
	s["SeparatorTextBorderSize"] = style.SeparatorTextBorderSize;
	s["SeparatorTextAlign"] = {style.SeparatorTextAlign.x, style.SeparatorTextAlign.y};
	s["SeparatorTextPadding"] = {style.SeparatorTextPadding.x, style.SeparatorTextPadding.y};
	s["DisplayWindowPadding"] = {style.DisplayWindowPadding.x, style.DisplayWindowPadding.y};
	s["DisplaySafeAreaPadding"] = {style.DisplaySafeAreaPadding.x, style.DisplaySafeAreaPadding.y};
	s["DockingSeparatorSize"] = style.DockingSeparatorSize;
	s["MouseCursorScale"] = style.MouseCursorScale;
	s["AntiAliasedLines"] = style.AntiAliasedLines;
	s["AntiAliasedLinesUseTex"] = style.AntiAliasedLinesUseTex;
	s["AntiAliasedFill"] = style.AntiAliasedFill;
	s["CurveTessellationTol"] = style.CurveTessellationTol;
	s["CircleTessellationMaxError"] = style.CircleTessellationMaxError;
	s["HoverStationaryDelay"] = style.HoverStationaryDelay;
	s["HoverDelayShort"] = style.HoverDelayShort;
	s["HoverDelayNormal"] = style.HoverDelayNormal;
	j["style"] = std::move(s);
	return j;
}

bool jsonToStyle(const json& j, ImGuiStyle& style, int* outBaseTheme) {
	if (!j.is_object()) {
		return false;
	}
	if (outBaseTheme && j.contains("baseTheme")) {
		*outBaseTheme = j["baseTheme"].get<int>();
	}

	if (j.contains("colors") && j["colors"].is_object()) {
		std::unordered_map<std::string, int> nameToCol;
		nameToCol.reserve(static_cast<size_t>(ImGuiCol_COUNT));
		for (int i = 0; i < ImGuiCol_COUNT; ++i) {
			nameToCol[ImGui::GetStyleColorName(i)] = i;
		}
		for (auto it = j["colors"].begin(); it != j["colors"].end(); ++it) {
			auto found = nameToCol.find(it.key());
			if (found == nameToCol.end() || !it.value().is_array() || it.value().size() < 4) {
				continue;
			}
			const auto& a = it.value();
			style.Colors[found->second] =
				ImVec4(a[0].get<float>(), a[1].get<float>(), a[2].get<float>(), a[3].get<float>());
		}
	}

	if (j.contains("style") && j["style"].is_object()) {
		const auto& s = j["style"];
		auto setF = [&](const char* key, float& dst) {
			if (s.contains(key)) {
				dst = s[key].get<float>();
			}
		};
		auto setV2 = [&](const char* key, ImVec2& dst) {
			if (s.contains(key) && s[key].is_array() && s[key].size() >= 2) {
				dst = ImVec2(s[key][0].get<float>(), s[key][1].get<float>());
			}
		};
		auto setI = [&](const char* key, int& dst) {
			if (s.contains(key)) {
				dst = s[key].get<int>();
			}
		};
		auto setB = [&](const char* key, bool& dst) {
			if (s.contains(key)) {
				dst = s[key].get<bool>();
			}
		};
		setF("Alpha", style.Alpha);
		setF("DisabledAlpha", style.DisabledAlpha);
		setV2("WindowPadding", style.WindowPadding);
		setF("WindowRounding", style.WindowRounding);
		setF("WindowBorderSize", style.WindowBorderSize);
		setF("WindowBorderHoverPadding", style.WindowBorderHoverPadding);
		setV2("WindowMinSize", style.WindowMinSize);
		setV2("WindowTitleAlign", style.WindowTitleAlign);
		{
			int v = static_cast<int>(style.WindowMenuButtonPosition);
			setI("WindowMenuButtonPosition", v);
			style.WindowMenuButtonPosition = static_cast<ImGuiDir>(v);
		}
		setF("ChildRounding", style.ChildRounding);
		setF("ChildBorderSize", style.ChildBorderSize);
		setF("PopupRounding", style.PopupRounding);
		setF("PopupBorderSize", style.PopupBorderSize);
		setV2("FramePadding", style.FramePadding);
		setF("FrameRounding", style.FrameRounding);
		setF("FrameBorderSize", style.FrameBorderSize);
		setV2("ItemSpacing", style.ItemSpacing);
		setV2("ItemInnerSpacing", style.ItemInnerSpacing);
		setV2("CellPadding", style.CellPadding);
		setV2("TouchExtraPadding", style.TouchExtraPadding);
		setF("IndentSpacing", style.IndentSpacing);
		setF("ColumnsMinSpacing", style.ColumnsMinSpacing);
		setF("ScrollbarSize", style.ScrollbarSize);
		setF("ScrollbarRounding", style.ScrollbarRounding);
		setF("GrabMinSize", style.GrabMinSize);
		setF("GrabRounding", style.GrabRounding);
		setF("LogSliderDeadzone", style.LogSliderDeadzone);
		setF("ImageBorderSize", style.ImageBorderSize);
		setF("TabRounding", style.TabRounding);
		setF("TabBorderSize", style.TabBorderSize);
		setF("TabCloseButtonMinWidthSelected", style.TabCloseButtonMinWidthSelected);
		setF("TabCloseButtonMinWidthUnselected", style.TabCloseButtonMinWidthUnselected);
		setF("TabBarBorderSize", style.TabBarBorderSize);
		setF("TabBarOverlineSize", style.TabBarOverlineSize);
		setF("TableAngledHeadersAngle", style.TableAngledHeadersAngle);
		setV2("TableAngledHeadersTextAlign", style.TableAngledHeadersTextAlign);
		{
			int v = static_cast<int>(style.TreeLinesFlags);
			setI("TreeLinesFlags", v);
			style.TreeLinesFlags = static_cast<ImGuiTreeNodeFlags>(v);
		}
		setF("TreeLinesSize", style.TreeLinesSize);
		setF("TreeLinesRounding", style.TreeLinesRounding);
		{
			int v = static_cast<int>(style.ColorButtonPosition);
			setI("ColorButtonPosition", v);
			style.ColorButtonPosition = static_cast<ImGuiDir>(v);
		}
		setV2("ButtonTextAlign", style.ButtonTextAlign);
		setV2("SelectableTextAlign", style.SelectableTextAlign);
		setF("SeparatorTextBorderSize", style.SeparatorTextBorderSize);
		setV2("SeparatorTextAlign", style.SeparatorTextAlign);
		setV2("SeparatorTextPadding", style.SeparatorTextPadding);
		setV2("DisplayWindowPadding", style.DisplayWindowPadding);
		setV2("DisplaySafeAreaPadding", style.DisplaySafeAreaPadding);
		setF("DockingSeparatorSize", style.DockingSeparatorSize);
		setF("MouseCursorScale", style.MouseCursorScale);
		setB("AntiAliasedLines", style.AntiAliasedLines);
		setB("AntiAliasedLinesUseTex", style.AntiAliasedLinesUseTex);
		setB("AntiAliasedFill", style.AntiAliasedFill);
		setF("CurveTessellationTol", style.CurveTessellationTol);
		setF("CircleTessellationMaxError", style.CircleTessellationMaxError);
		setF("HoverStationaryDelay", style.HoverStationaryDelay);
		setF("HoverDelayShort", style.HoverDelayShort);
		setF("HoverDelayNormal", style.HoverDelayNormal);
	}
	return true;
}

bool saveStyleToFile(const std::string& path, const ImGuiStyle& style, int baseTheme) {
	if (path.empty()) {
		return false;
	}
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
	std::ofstream out(path);
	if (!out.is_open()) {
		spdlog::error("[rigImGui] Failed to save theme to {}", path);
		return false;
	}
	out << styleToJson(style, baseTheme).dump(2);
	spdlog::info("[rigImGui] Saved theme to {}", path);
	return true;
}

bool loadStyleFromFile(const std::string& path, ImGuiStyle& style, int* outBaseTheme) {
	if (path.empty()) {
		return false;
	}
	std::ifstream in(path);
	if (!in.is_open()) {
		spdlog::warn("[rigImGui] Theme file not found: {}", path);
		return false;
	}
	try {
		json j;
		in >> j;
		if (!jsonToStyle(j, style, outBaseTheme)) {
			spdlog::error("[rigImGui] Invalid theme JSON: {}", path);
			return false;
		}
		spdlog::info("[rigImGui] Loaded theme from {}", path);
		return true;
	} catch (const std::exception& e) {
		spdlog::error("[rigImGui] Theme load error ({}): {}", path, e.what());
		return false;
	}
}

bool loadFonts(ImGuiIO& io, const std::string& fontsSearchDir, const std::string& bodyFontPath,
			   float sizePixels) {
	float uiSize = sizePixels;
	if (uiSize < 8.0f) {
		uiSize = 8.0f;
	}
	if (uiSize > 48.0f) {
		uiSize = 48.0f;
	}

	bool loadedBody = false;
	namespace fs = std::filesystem;

	// Basic Latin + Latin-1, plus common UI punctuation (—, …, →) that otherwise
	// shows as missing-glyph diamonds / question marks with default ranges.
	static const ImWchar kBodyGlyphRanges[] = {
		0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement
		0x2010, 0x2027, // hyphen / en-dash / em-dash / ellipsis
		0x2190, 0x2193, // ← ↑ → ↓
		0,
	};

	auto tryAddFile = [&](const fs::path& p) -> bool {
		std::error_code ec;
		if (!fs::is_regular_file(p, ec)) {
			return false;
		}
		if (io.Fonts->AddFontFromFileTTF(p.string().c_str(), uiSize, nullptr, kBodyGlyphRanges)) {
			spdlog::info("[rigImGui] Loaded UI font from {}", p.string());
			return true;
		}
		return false;
	};

	if (!bodyFontPath.empty()) {
		fs::path custom(bodyFontPath);
		if (custom.is_absolute()) {
			loadedBody = tryAddFile(custom);
		} else {
			loadedBody = tryAddFile(fs::path(fontsSearchDir) / custom);
			if (!loadedBody) {
				loadedBody = tryAddFile(custom);
			}
		}
		if (!loadedBody) {
			spdlog::warn("[rigImGui] Custom font not found: {} — falling back to Roboto",
						 bodyFontPath);
		}
	}

	if (!loadedBody) {
		const auto robotoPath = findFontFile(fontsSearchDir, "Roboto-Regular.ttf");
		if (!robotoPath.empty()) {
			loadedBody = tryAddFile(robotoPath);
		}
	}

	if (!loadedBody && sizeof(fontRobotoRegular) > 32) {
		ImFontConfig cfg;
		cfg.FontDataOwnedByAtlas = false;
		io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(fontRobotoRegular),
									   static_cast<int>(sizeof(fontRobotoRegular)), uiSize, &cfg,
									   kBodyGlyphRanges);
		loadedBody = true;
		spdlog::info("[rigImGui] Loaded embedded Roboto UI font");
	}

	if (!loadedBody) {
		io.Fonts->AddFontDefault();
		spdlog::warn("[rigImGui] No body TTF — using ImGui default font");
	}

#if RIGIMGUI_HAS_ICON_HEADERS
	const auto faPath = findFontFile(fontsSearchDir, FONT_ICON_FILE_NAME_FAS);
	if (!faPath.empty()) {
		ImFontConfig icons;
		icons.MergeMode = true;
		icons.PixelSnapH = true;
		icons.GlyphMinAdvanceX = uiSize;
		static const ImWchar ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
		io.Fonts->AddFontFromFileTTF(faPath.string().c_str(), uiSize, &icons, ranges);
		spdlog::info("[rigImGui] Merged Font Awesome from {}", faPath.string());
	} else {
		spdlog::warn(
			"[rigImGui] No {} — IconFont headers present but glyphs inactive",
			FONT_ICON_FILE_NAME_FAS);
	}
#else
	(void)fontsSearchDir;
#endif
	return loadedBody;
}

std::string resolveBodyFontPath(const std::string& fontsSearchDir, const std::string& bodyFontPath) {
	namespace fs = std::filesystem;
	std::error_code ec;
	auto existsFile = [&](const fs::path& p) -> std::string {
		if (fs::is_regular_file(p, ec)) {
			return fs::weakly_canonical(p, ec).string();
		}
		return {};
	};

	if (!bodyFontPath.empty()) {
		fs::path custom(bodyFontPath);
		if (custom.is_absolute()) {
			if (auto s = existsFile(custom); !s.empty()) {
				return s;
			}
		} else {
			if (auto s = existsFile(fs::path(fontsSearchDir) / custom); !s.empty()) {
				return s;
			}
			if (auto s = existsFile(custom); !s.empty()) {
				return s;
			}
		}
	}

	const auto roboto = findFontFile(fontsSearchDir, "Roboto-Regular.ttf");
	if (!roboto.empty()) {
		return roboto.string();
	}
	return {};
}

} // namespace ImGuiStyleKit
} // namespace rigkit

