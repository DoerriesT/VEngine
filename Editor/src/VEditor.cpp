#include "VEditor.h"
#include <Engine.h>
#include <Components/TransformationComponent.h>
#include <Components/CameraComponent.h>
#include <Graphics/RenderSystem.h>
#include <Graphics/imgui/imgui.h>
#include "EntityDetailWindow.h"
#include "EntityWindow.h"
#include "AssetBrowserWindow.h"

using namespace VEngine;

extern entt::entity g_emitterEntity;
extern entt::entity g_localLightEntity;

VEditor::VEditor::VEditor(VEngine::IGameLogic &gameLogic)
	:m_gameLogic(gameLogic),
	m_engine(),
	m_editorCameraEntity(),
	m_entityDetailWindow()
{
}

void VEditor::VEditor::initialize(VEngine::Engine *engine)
{
	m_engine = engine;

	uint32_t width = m_engine->getWindowWidth();
	uint32_t height = m_engine->getWindowHeight();

	auto &registry = m_engine->getEntityRegistry();
	m_editorCameraEntity = registry.create();
	registry.assign<TransformationComponent>(m_editorCameraEntity, TransformationComponent::Mobility::DYNAMIC, glm::vec3(-12.0f, 1.8f, 0.0f), glm::quat(glm::vec3(0.0f, glm::radians(90.0f), 0.0f)));
	registry.assign<CameraComponent>(m_editorCameraEntity, CameraComponent::ControllerType::FPS, (width > 0 && height > 0) ? width / (float)height : 1.0f, glm::radians(60.0f), 0.1f, 300.0f);


	m_entityDetailWindow = new EntityDetailWindow(m_engine);
	m_entityDetailWindow->setVisible(true);
	m_entityWindow = new EntityWindow(m_engine);
	m_entityWindow->setVisible(true);
	m_assetBrowserWindow = new AssetBrowserWindow(m_engine);
	m_assetBrowserWindow->setVisible(true);

	m_engine->getRenderSystem().setCameraEntity(m_editorCameraEntity);

	m_gameLogic.initialize(engine);

	m_engine->getRenderSystem().setCameraEntity(m_editorCameraEntity);
}

void VEditor::VEditor::update(float timeDelta)
{
	m_gameLogic.update(timeDelta);

	// make sure aspect ratio of editor camera is correct
	{
		uint32_t width = m_engine->getWindowWidth();
		uint32_t height = m_engine->getWindowHeight();
		m_engine->getEntityRegistry().get<CameraComponent>(m_editorCameraEntity).m_aspectRatio = (width > 0 && height > 0) ? width / (float)height : 1.0f;
	}

	// make sure we use the editor camera
	m_engine->getRenderSystem().setCameraEntity(m_editorCameraEntity);
	

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
		//if (ImGui::BeginMenu("File"))
		//{
		//	// ShowExampleMenuFile();
		//	ImGui::EndMenu();
		//}
		//if (ImGui::BeginMenu("Edit"))
		//{
		//	if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
		//	if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
		//	ImGui::Separator();
		//	if (ImGui::MenuItem("Cut", "CTRL+X")) {}
		//	if (ImGui::MenuItem("Copy", "CTRL+C")) {}
		//	if (ImGui::MenuItem("Paste", "CTRL+V")) {}
		//	ImGui::EndMenu();
		//}
		if (ImGui::BeginMenu("Window"))
		{
			//ImGui::MenuItem("Entities", "", &m_showEntityWindow);
			//ImGui::MenuItem("Details", "", &m_showEntityDetailWindow);
			//ImGui::MenuItem("Profiler", "", &m_showProfilerWindow);
			//ImGui::MenuItem("Memory", "", &m_showMemoryWindow);
			//ImGui::MenuItem("Exposure Settings", "", &m_showExposureWindow);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	ImGui::End();

	//memmove(m_frametimes.get(), m_frametimes.get() + 1, (FRAME_TIME_ARRAY_SIZE - 1) * sizeof(float));
	//m_frametimes[FRAME_TIME_ARRAY_SIZE - 1] = timeDelta * 1000.0f;

	//if (m_showProfilerWindow)
	//{
	//	ImGui::Begin("Profiler");
	//	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	//
	//	char overlay[32];
	//	sprintf_s(overlay, "%10.10f ms", m_frametimes[FRAME_TIME_ARRAY_SIZE - 1]);
	//	ImGui::PlotLines("Frametime", m_frametimes.get(), FRAME_TIME_ARRAY_SIZE, 0, overlay, 0.0f, 50.0f, ImVec2(0, 80));
	//
	//	ImGui::Separator();
	//	{
	//		size_t passTimingCount;
	//		const PassTimingInfo *passTimingInfo;
	//		renderSystem.getTimingInfo(&passTimingCount, &passTimingInfo);
	//		float total = 0.0f;
	//		for (size_t i = 0; i < passTimingCount; ++i)
	//		{
	//			ImGui::Text("%30s : %f ms", passTimingInfo[i].m_passName, passTimingInfo[i].m_passTimeWithSync);
	//			total += passTimingInfo[i].m_passTimeWithSync;
	//		}
	//		const char *totalStr = "Total";
	//		ImGui::Text("%30s : %f ms", totalStr, total);
	//	}
	//
	//	ImGui::End();
	//}

	//if (m_showMemoryWindow)
	//{
	//	ImGui::Begin("VRAM Allocator");
	//
	//	//float(*memoryPlotValuesGetter)(void *data, int idx, int *type) = [](void *data, int idx, int *type) -> float
	//	//{
	//	//	const VKMemoryBlockDebugInfo *debugInfo = (VKMemoryBlockDebugInfo *)data;
	//	//	const auto &span = debugInfo->m_spans[idx];
	//	//	*type = (int)span.m_state;
	//	//	return static_cast<float>(span.m_offset + span.m_size) / debugInfo->m_allocationSize;
	//	//};
	//	//
	//	//auto memoryInfo = renderSystem.getMemoryAllocatorDebugInfo();
	//	//uint32_t currentMemoryType = ~uint32_t();
	//	//for (auto &info : memoryInfo)
	//	//{
	//	//	if (info.m_memoryType != currentMemoryType)
	//	//	{
	//	//		ImGui::Separator();
	//	//		ImGui::Text("Memory Type %u", info.m_memoryType);
	//	//		currentMemoryType = info.m_memoryType;
	//	//	}
	//	//	plotMemoryAllocator("", memoryPlotValuesGetter, &info, info.m_spans.size(), nullptr, {});
	//	//	uint32_t freeUsedWasted[3]{};
	//	//	uint32_t allocCount = 0;
	//	//	for (auto &span : info.m_spans)
	//	//	{
	//	//		freeUsedWasted[(size_t)span.m_state] += span.m_size;
	//	//		allocCount += span.m_state == TLSFSpanDebugInfo::State::USED ? 1 : 0;
	//	//	}
	//	//	float freeUsedWastedMB[3];
	//	//	float total = 0.0f;
	//	//	for (size_t i = 0; i < 3; ++i)
	//	//	{
	//	//		freeUsedWastedMB[i] = freeUsedWasted[i] * (1.0f / (1024.0f * 1024.0f));
	//	//		total += freeUsedWastedMB[i];
	//	//	}
	//	//	float toPercent = 100.0f * (1.0f / total);
	//	//	ImGui::Text("Free: %7.3f MB (%6.2f %%) | Used: %7.3f MB (%6.2f %%) | Total: %7.3f MB | Allocations: %u", freeUsedWastedMB[0], freeUsedWastedMB[0] * toPercent, freeUsedWastedMB[1], freeUsedWastedMB[1] * toPercent, total, allocCount);
	//	//}
	//
	//	ImGui::End();
	//}

	//if (m_showExposureWindow)
	//{
	//	ImGui::Begin("Exposure Settings");
	//
	//	float exposureHistogramLogMin = g_ExposureHistogramLogMin;
	//	float exposureHistogramLogMax = g_ExposureHistogramLogMax;
	//	float exposureLowPercentage = g_ExposureLowPercentage * 100.0f;
	//	float exposureHighPercentage = g_ExposureHighPercentage * 100.0f;
	//	float exposureSpeedUp = g_ExposureSpeedUp;
	//	float exposureSpeedDown = g_ExposureSpeedDown;
	//	float exposureCompensation = g_ExposureCompensation;
	//	float exposureMin = g_ExposureMin;
	//	float exposureMax = g_ExposureMax;
	//	bool exposureFixed = g_ExposureFixed;
	//	float exposureFixedValue = g_ExposureFixedValue;
	//
	//	ImGui::DragFloat("Histogram Log Min", &exposureHistogramLogMin, 0.05f, -14.0f, 17.0f);
	//	ImGui::DragFloat("Histogram Log Max", &exposureHistogramLogMax, 0.05f, -14.0f, 17.0f);
	//	ImGui::DragFloat("Low Percentage", &exposureLowPercentage, 0.5f, 0.0f, 100.0f);
	//	ImGui::DragFloat("High Percentage", &exposureHighPercentage, 0.5f, 0.0f, 100.0f);
	//	ImGui::DragFloat("Speed Up", &exposureSpeedUp, 0.05f, 0.01f, 10.0f);
	//	ImGui::DragFloat("Speed Down", &exposureSpeedDown, 0.05f, 0.01f, 10.0f);
	//	ImGui::DragFloat("Exposure Compensation", &exposureCompensation, 0.25f, -10.0f, 10.0f);
	//	ImGui::DragFloat("Exposure Min", &exposureMin, 0.05f, 0.001f, 1000.0f);
	//	ImGui::DragFloat("Exposure Max", &exposureMax, 0.05f, 0.001f, 1000.0f);
	//	ImGui::Checkbox("Fixed Exposure", &exposureFixed);
	//	ImGui::DragFloat("Fixed Exposure Value", &exposureFixedValue, 0.25f, 0.01f, 1000.0f);
	//
	//	g_ExposureHistogramLogMin = exposureHistogramLogMin;
	//	g_ExposureHistogramLogMax = exposureHistogramLogMax;
	//	g_ExposureLowPercentage = exposureLowPercentage * 0.01f;
	//	g_ExposureHighPercentage = exposureHighPercentage * 0.01f;
	//	g_ExposureSpeedUp = exposureSpeedUp;
	//	g_ExposureSpeedDown = exposureSpeedDown;
	//	g_ExposureCompensation = exposureCompensation;
	//	g_ExposureMin = exposureMin;
	//	g_ExposureMax = exposureMax;
	//	g_ExposureFixed = exposureFixed;
	//	g_ExposureFixedValue = exposureFixedValue;
	//
	//	const uint32_t *luminancHistogram = renderSystem.getLuminanceHistogram();
	//	float(*histogramValuesGetter)(void *data, int idx) = [](void *data, int idx) -> float
	//	{
	//		const uint32_t *luminancHistogram = *(const uint32_t **)data;
	//		return static_cast<float>(luminancHistogram[idx]);
	//	};
	//	ImGui::PlotHistogram("Luminance Histogram", histogramValuesGetter, (void *)&luminancHistogram, RendererConsts::LUMINANCE_HISTOGRAM_SIZE, 0, nullptr, FLT_MAX, FLT_MAX, { 0, 80 });
	//
	//	ImGui::End();
	//}

	if (m_entityWindow->isVisible())
	{
		m_entityWindow->draw();
	}

	if (m_entityDetailWindow->isVisible())
	{
		m_entityDetailWindow->draw(m_entityWindow->getSelectedEntity(), m_editorCameraEntity);
	}

	// asset browser
	if (m_assetBrowserWindow->isVisible())
	{
		m_assetBrowserWindow->draw();
	}


	// viewport
	{
		ImGui::Begin("Viewport");

		ImGui::End();
	}

	// modes
	{
		ImGui::Begin("Modes");

		ImGui::End();
	}

	// quick menu
	{
		ImGui::Begin("Quick Menu");

		ImGui::End();
	}
}

void VEditor::VEditor::shutdown()
{
	m_gameLogic.shutdown();
}
