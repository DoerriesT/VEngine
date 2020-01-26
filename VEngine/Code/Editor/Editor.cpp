#include "Editor.h"
#include "Engine.h"
#include "Components/TransformationComponent.h"
#include "Components/CameraComponent.h"
#include "EntityDetailsWindow.h"
#include "Graphics/imgui/imgui.h"
#include "Gui/MemoryAllocatorPlot.h"
#include "Graphics/PassTimingInfo.h"
#include "Graphics/RenderSystem.h"

extern uint32_t g_dirLightEntity;
static constexpr size_t FRAME_TIME_ARRAY_SIZE = 64;

VEngine::Editor::Editor(Engine *engine, uint32_t editorWindowWidth, uint32_t editorWindowHeight)
	:m_engine(engine),
	m_detailsWindow(std::make_unique<EntityDetailsWindow>(engine)),
	m_frametimes(std::make_unique<float[]>(FRAME_TIME_ARRAY_SIZE)),
	m_width(editorWindowWidth),
	m_height(editorWindowHeight)
{
	memset(m_frametimes.get(), 0, sizeof(float) * FRAME_TIME_ARRAY_SIZE);
	auto &registry = m_engine->getEntityRegistry();
	m_editorCameraEntity = registry.create();
	registry.assign<TransformationComponent>(m_editorCameraEntity, TransformationComponent::Mobility::DYNAMIC, glm::vec3(-12.0f, 1.8f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(90.0f), 0.0f)));
	registry.assign<CameraComponent>(m_editorCameraEntity, CameraComponent::ControllerType::FPS, (m_width > 0 && m_height > 0) ? m_width / (float)m_height : 1.0f, glm::radians(60.0f), 0.1f, 300.0f);
}

VEngine::Editor::~Editor()
{
}

void VEngine::Editor::update(float timeDelta)
{
	auto &renderSystem = m_engine->getRenderSystem();

	// begin main dock space/main menu bar
	{
		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		window_flags |= ImGuiWindowFlags_NoBackground;
		ImGuiViewport *viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("MainDockSpace", nullptr, window_flags);
		ImGui::PopStyleVar();
		ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpaceID");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	}


	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			// ShowExampleMenuFile();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
			if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "CTRL+X")) {}
			if (ImGui::MenuItem("Copy", "CTRL+C")) {}
			if (ImGui::MenuItem("Paste", "CTRL+V")) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Entities", "", &m_showEntityWindow);
			ImGui::MenuItem("Details", "", &m_showEntityDetailWindow);
			ImGui::MenuItem("Profiler", "", &m_showProfilerWindow);
			ImGui::MenuItem("Memory", "", &m_showMemoryWindow);
			ImGui::MenuItem("Luminance Histogram", "", &m_showLuminanceHistogramWindow);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::End();

	memmove(m_frametimes.get(), m_frametimes.get() + 1, (FRAME_TIME_ARRAY_SIZE - 1) * sizeof(float));
	m_frametimes[FRAME_TIME_ARRAY_SIZE - 1] = timeDelta * 1000.0f;

	if (m_showProfilerWindow)
	{
		ImGui::Begin("Profiler");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		char overlay[32];
		sprintf_s(overlay, "%10.10f ms", m_frametimes[FRAME_TIME_ARRAY_SIZE - 1]);
		ImGui::PlotLines("Frametime", m_frametimes.get(), FRAME_TIME_ARRAY_SIZE, 0, overlay, 0.0f, 50.0f, ImVec2(0, 80));

		ImGui::Separator();
		{
			size_t passTimingCount;
			const PassTimingInfo *passTimingInfo;
			renderSystem.getTimingInfo(&passTimingCount, &passTimingInfo);
			float total = 0.0f;
			for (size_t i = 0; i < passTimingCount; ++i)
			{
				ImGui::Text("%30s : %f ms", passTimingInfo[i].m_passName, passTimingInfo[i].m_passTimeWithSync);
				total += passTimingInfo[i].m_passTimeWithSync;
			}
			const char *totalStr = "Total";
			ImGui::Text("%30s : %f ms", totalStr, total);
		}

		ImGui::End();
	}

	if (m_showMemoryWindow)
	{
		ImGui::Begin("VRAM Allocator");

		float(*memoryPlotValuesGetter)(void *data, int idx, int *type) = [](void *data, int idx, int *type) -> float
		{
			const VKMemoryBlockDebugInfo *debugInfo = (VKMemoryBlockDebugInfo *)data;
			const auto &span = debugInfo->m_spans[idx];
			*type = (int)span.m_state;
			return static_cast<float>(span.m_offset + span.m_size) / debugInfo->m_allocationSize;
		};

		auto memoryInfo = renderSystem.getMemoryAllocatorDebugInfo();
		uint32_t currentMemoryType = ~uint32_t();
		for (auto &info : memoryInfo)
		{
			if (info.m_memoryType != currentMemoryType)
			{
				ImGui::Separator();
				ImGui::Text("Memory Type %u", info.m_memoryType);
				currentMemoryType = info.m_memoryType;
			}
			plotMemoryAllocator("", memoryPlotValuesGetter, &info, info.m_spans.size(), nullptr, {});
			uint32_t freeUsedWasted[3]{};
			uint32_t allocCount = 0;
			for (auto &span : info.m_spans)
			{
				freeUsedWasted[(size_t)span.m_state] += span.m_size;
				allocCount += span.m_state == TLSFSpanDebugInfo::State::USED ? 1 : 0;
			}
			float freeUsedWastedMB[3];
			float total = 0.0f;
			for (size_t i = 0; i < 3; ++i)
			{
				freeUsedWastedMB[i] = freeUsedWasted[i] * (1.0f / (1024.0f * 1024.0f));
				total += freeUsedWastedMB[i];
			}
			float toPercent = 100.0f * (1.0f / total);
			ImGui::Text("Free: %7.3f MB (%6.2f %%) | Used: %7.3f MB (%6.2f %%) | Total: %7.3f MB | Allocations: %u", freeUsedWastedMB[0], freeUsedWastedMB[0] * toPercent, freeUsedWastedMB[1], freeUsedWastedMB[1] * toPercent, total, allocCount);
		}

		ImGui::End();
	}

	if (m_showLuminanceHistogramWindow)
	{
		ImGui::Begin("Luminance Histogram");

		const uint32_t *luminancHistogram = renderSystem.getLuminanceHistogram();
		float(*histogramValuesGetter)(void *data, int idx) = [](void *data, int idx) -> float
		{
			const uint32_t *luminancHistogram = *(const uint32_t **)data;
			return static_cast<float>(luminancHistogram[idx]);
		};
		ImGui::PlotHistogram("Luminance Histogram", histogramValuesGetter, (void *)&luminancHistogram, RendererConsts::LUMINANCE_HISTOGRAM_SIZE, 0, nullptr, FLT_MAX, FLT_MAX, { 0, 80 });

		ImGui::End();
	}

	if (m_showEntityWindow)
	{
		ImGui::Begin("Entities");

		ImGui::End();
	}

	if (m_showEntityDetailWindow)
	{
		m_detailsWindow->draw(g_dirLightEntity, m_editorCameraEntity);
	}
}

uint32_t VEngine::Editor::getEditorCameraEntity() const
{
	return m_editorCameraEntity;
}

void VEngine::Editor::resize(uint32_t editorWindowWidth, uint32_t editorWindowHeight)
{
	m_width = editorWindowWidth;
	m_height = editorWindowHeight;
	auto &registry = m_engine->getEntityRegistry();
	registry.get<CameraComponent>(m_editorCameraEntity).m_aspectRatio = (m_width > 0 && m_height > 0) ? m_width / (float)m_height : 1.0f;
}
