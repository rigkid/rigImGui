#include "DebugPanel.h"

#include "core/IMui.h"
#include "core/RigKitEngine.h"
#include "core/util/Progress.h"
#include "ecs/MEcs.h"

#include <cstdio>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace rigkit {
namespace {

bool readProcessRssMb(double& outMb) {
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS_EX pmc{};
	if (!GetProcessMemoryInfo(GetCurrentProcess(),
							  reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
							  sizeof(pmc))) {
		return false;
	}
	outMb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
	return true;
#elif defined(__linux__)
	FILE* f = std::fopen("/proc/self/status", "r");
	if (!f)
		return false;
	char line[256];
	long kb = -1;
	while (std::fgets(line, sizeof(line), f)) {
		if (std::sscanf(line, "VmRSS: %ld", &kb) == 1)
			break;
	}
	std::fclose(f);
	if (kb < 0)
		return false;
	outMb = static_cast<double>(kb) / 1024.0;
	return true;
#else
	(void)outMb;
	return false;
#endif
}

} // namespace


DebugPanel::DebugPanel(const std::string &title, ImGuiWindowFlags flags)
	: IWindow(title, flags) {}

void DebugPanel::renderContents() {
	if (m_engine) {
		tickProgressDemo(m_engine->getDeltaTime());
	}
	renderDebugInfo();
	renderClearShapesButton();
	renderEntityInfo();
	renderProgressDemo();
	renderPerformanceInfo();
}

void DebugPanel::renderDebugInfo() {
	if (!m_engine)
		return;

	float deltaTime = m_engine->getDeltaTime();
	ImGui::Text("Debug Info:");
	ImGui::Text("  Delta Time: %.3f ms", deltaTime * 1000.0f);
	ImGui::Text("  Frame Rate: %.1f FPS", 1.0f / deltaTime);
	if (IMui* ui = m_engine->getUiManager()) {
		ImGui::Text("  Chrome kerning: %s (%d kern pairs)", ui->chromeKerning() ? "on" : "off",
					ui->chromeKernPairCount());
		ImGui::TextUnformatted("  AV To LiveFace");
	}
}

void DebugPanel::renderClearShapesButton() {
	if (!m_engine)
		return;

	auto *ecs = m_engine->getECSManager();
	if (!ecs)
		return;

	if (ImGui::Button("Clear All Entities")) {
		auto all = ecs->getAllEntities();
		for (auto entity : all) {
			ecs->destroyEntity(entity);
		}
		spdlog::debug("[DebugPanel] Cleared {} entities", all.size());
	}
}

void DebugPanel::renderEntityInfo() {
	if (!m_engine)
		return;

	auto *ecs = m_engine->getECSManager();
	if (!ecs)
		return;

	ImGui::Separator();
	ImGui::Text("Entity Info:");
	ImGui::Text("  Total Entities: %zu", ecs->getEntityCount());
	ImGui::Text("  Registered component types: %zu",
				ecs->componentTypes().size());
	for (const auto &info : ecs->componentTypes()) {
		ImGui::BulletText("%s%s", info.name.c_str(),
						  info.portable ? "" : " (host)");
	}
}

void DebugPanel::renderProgressDemo() {
	if (!m_engine)
		return;

	IMui *ui = m_engine->getUiManager();
	Progress *progress = ui ? ui->progress() : nullptr;
	if (!progress)
		return;

	ImGui::Separator();
	ImGui::TextUnformatted("Progress");
	ImGui::TextDisabled("Fake loaders - status bar or floating (Preferences).");
	ImGui::SetNextItemWidth(160.f);
	ImGui::SliderFloat("Sim speed", &m_simulationSpeed, 0.1f, 5.f, "%.1fx");

	const bool stepRunning = (m_progressDemo == ProgressDemo::StepBased);
	if (stepRunning)
		ImGui::BeginDisabled();
	if (ImGui::Button("Step-based (80 steps)", ImVec2(-1, 0))) {
		m_progressDemo = ProgressDemo::StepBased;
		m_stepCurrent = 0;
		m_stepAccumulator = 0.f;
		progress->setCancelable(true);
		progress->begin("Exporting layers", m_stepTotal);
	}
	if (stepRunning)
		ImGui::EndDisabled();

	const bool absRunning = (m_progressDemo == ProgressDemo::Absolute);
	if (absRunning)
		ImGui::BeginDisabled();
	if (ImGui::Button("Absolute progress", ImVec2(-1, 0))) {
		m_progressDemo = ProgressDemo::Absolute;
		m_absolutePos = 0.f;
		progress->setCancelable(true);
		progress->begin("Rendering frames");
	}
	if (absRunning)
		ImGui::EndDisabled();

	const bool indRunning = (m_progressDemo == ProgressDemo::Indeterminate);
	if (indRunning)
		ImGui::BeginDisabled();
	if (ImGui::Button("Indeterminate (4 s)", ImVec2(-1, 0))) {
		m_progressDemo = ProgressDemo::Indeterminate;
		m_absolutePos = 0.f;
		progress->setCancelable(true);
		progress->begin("Connecting...");
		progress->tickIndeterminate("Please wait...");
	}
	if (indRunning)
		ImGui::EndDisabled();

	const bool anyRunning = (m_progressDemo != ProgressDemo::None);
	if (!anyRunning)
		ImGui::BeginDisabled();
	if (ImGui::Button("Cancel / hide", ImVec2(-1, 0))) {
		m_progressDemo = ProgressDemo::None;
		progress->hide();
	}
	if (!anyRunning)
		ImGui::EndDisabled();
}

void DebugPanel::tickProgressDemo(float dt) {
	if (!m_engine || m_progressDemo == ProgressDemo::None)
		return;

	IMui *ui = m_engine->getUiManager();
	Progress *progress = ui ? ui->progress() : nullptr;
	if (!progress)
		return;

	if (progress->cancelRequested()) {
		m_progressDemo = ProgressDemo::None;
		progress->hide();
		return;
	}

	const float speed = m_simulationSpeed;

	switch (m_progressDemo) {
	case ProgressDemo::StepBased: {
		m_stepAccumulator += dt * speed * 12.f;
		while (m_stepAccumulator >= 1.f && m_stepCurrent < m_stepTotal) {
			m_stepAccumulator -= 1.f;
			++m_stepCurrent;
			progress->tick("Layer " + std::to_string(m_stepCurrent) + " / " +
						   std::to_string(m_stepTotal));
		}
		if (m_stepCurrent >= m_stepTotal) {
			m_progressDemo = ProgressDemo::None;
			m_stepAccumulator = 0.f;
			progress->finish("Export complete");
		}
		break;
	}
	case ProgressDemo::Absolute: {
		m_absolutePos += dt * speed * 0.18f;
		if (m_absolutePos < 1.f) {
			const int frame = static_cast<int>(m_absolutePos * 120.f);
			progress->tick("Frame " + std::to_string(frame) + " / 120", m_absolutePos);
		} else {
			m_progressDemo = ProgressDemo::None;
			m_absolutePos = 0.f;
			progress->finish("Render done");
		}
		break;
	}
	case ProgressDemo::Indeterminate: {
		m_absolutePos += dt * speed;
		if (m_absolutePos < 4.f) {
			progress->tickIndeterminate("Please wait...");
		} else {
			m_progressDemo = ProgressDemo::None;
			m_absolutePos = 0.f;
			progress->finish("Connected");
		}
		break;
	}
	case ProgressDemo::None:
	default:
		break;
	}
}

void DebugPanel::renderPerformanceInfo() {
	ImGui::Separator();
	ImGui::Text("Performance:");
	double rssMb = 0.0;
	if (readProcessRssMb(rssMb)) {
		ImGui::Text("  Process RSS: %.1f MB", rssMb);
	} else {
		ImGui::TextUnformatted("  Process RSS: n/a");
	}
	// Portable GLES path has no reliable cross-vendor GPU budget query.
	ImGui::TextUnformatted("  GPU Memory: n/a");
}

} // namespace rigkit
