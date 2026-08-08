#include "ViewportWindow.h"

#include "Mui.h"
#include "Rulers.h"
#include "UiDpi.h"
#include "core/RigKitEngine.h"
#include "core/util/MSettings.h"
#include <imgui.h>

namespace rigkit {

ViewportWindow::ViewportWindow(const std::string& title) : IWindow(title) {
	setCategory("Host");
}

void ViewportWindow::setTexture(unsigned int textureId, int texW, int texH) {
	m_texture = textureId;
	m_texW = texW;
	m_texH = texH;
	if (texW > 0 && texH > 0) {
		m_view.contentSize = {static_cast<float>(texW), static_cast<float>(texH)};
	}
}

void ViewportWindow::clearTexture() {
	m_texture = 0;
	m_texW = 0;
	m_texH = 0;
}

void ViewportWindow::renderContents() {
	ImGui::Checkbox("Rulers", &m_rulers);
	ImGui::SameLine();
	if (ImGui::Button("Fit")) {
		m_view.fitToCanvas();
	}

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##viewport_canvas", uiHitSize(avail));
	const bool hovered = ImGui::IsItemHovered();
	m_view.hovered = hovered;
	m_view.canvasOrigin = {canvasPos.x, canvasPos.y};
	m_view.canvasW = avail.x;
	m_view.canvasH = avail.y;
	m_view.updateDerived();

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + avail.x, canvasPos.y + avail.y),
					  IM_COL32(28, 28, 32, 255));

	if (m_texture != 0 && m_texW > 0 && m_texH > 0) {
		const ImVec2 a = {m_view.ox, m_view.oy};
		const ImVec2 b = {m_view.ox + m_view.contentSize.x * m_view.zoomAbs,
						  m_view.oy + m_view.contentSize.y * m_view.zoomAbs};
		dl->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(m_texture)), a, b);
	} else {
		// Empty grid so the panel is usable before a texture is bound.
		const float step = 32.f * m_view.zoomAbs;
		if (step > 4.f) {
			for (float x = m_view.ox; x < canvasPos.x + avail.x; x += step) {
				dl->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + avail.y),
							IM_COL32(50, 50, 55, 255));
			}
			for (float y = m_view.oy; y < canvasPos.y + avail.y; y += step) {
				dl->AddLine(ImVec2(canvasPos.x, y), ImVec2(canvasPos.x + avail.x, y),
							IM_COL32(50, 50, 55, 255));
			}
		}
		dl->AddText(ImVec2(canvasPos.x + 8.f, canvasPos.y + 8.f), IM_COL32(140, 140, 140, 255),
					"No texture — call ViewportWindow::setTexture");
	}

	Mui* ui = nullptr;
	if (getEngine() && getEngine()->getUiManager()) {
		ui = dynamic_cast<Mui*>(getEngine()->getUiManager());
	}
	const float dpi = ui ? ui->dpiScale() : 1.f;
	int localUnit = 0;
	int& rulerUnit = ui ? ui->uiPrefs().rulerUnit : localUnit;
	const RulerUnit unit = clampRulerUnit(rulerUnit);

	if (m_rulers) {
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		const float ppu = rulerPixPerDisplayUnit(unit, m_view.zoomAbs, dpi);
		drawRulersInRegion(dl, canvasPos, avail, mouse, ppu, rulerUnitLabel(unit), dpi,
						   ImVec2(m_view.ox, m_view.oy));
		if (rulerHitStrip(canvasPos, avail, mouse, dpi) &&
			ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			ImGui::OpenPopup("##ViewportRulerUnits");
		}
	}
	if (rulerUnitPopup("##ViewportRulerUnits", rulerUnit)) {
		rulerUnit = static_cast<int>(clampRulerUnit(rulerUnit));
		if (ui && getEngine()) {
			if (auto* settings = getEngine()->getSettingsManager()) {
				settings->markDirty();
			}
		}
	}

	if (hovered) {
		const float wheel = ImGui::GetIO().MouseWheel;
		if (wheel != 0.f) {
			m_view.applyScrollZoom(wheel, ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
		}
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
			(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyAlt)) {
			m_panning = true;
		}
	}
	if (m_panning) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
			(ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::GetIO().KeyAlt)) {
			m_view.applyPanDelta(ImGui::GetIO().MouseDelta.x, ImGui::GetIO().MouseDelta.y);
		} else {
			m_panning = false;
		}
	}
}

} // namespace rigkit
