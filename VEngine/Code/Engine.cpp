#include "Engine.h"
#include "Window/Window.h"
#include "Input/UserInput.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "Graphics/Vulkan/VKContext.h"
#include "IGameLogic.h"
#include "Graphics/RenderSystem.h"
#include "Input/CameraControllerSystem.h"
#include "GlobalVar.h"
#include "Utility/Timer.h"
#include "graphics/imgui/imgui.h"
#include "graphics/imgui/imgui_impl_glfw.h"
#include "Graphics/imgui/ImGuizmo.h"
#include "Gui/MemoryAllocatorPlot.h"
#include "Graphics/PassTimingInfo.h"

uint32_t g_debugVoxelCascadeIndex = 0;
uint32_t g_giVoxelDebugMode = 0;

VEngine::Engine::Engine(const char *title, IGameLogic &gameLogic)
	:m_gameLogic(gameLogic),
	m_windowTitle(title)
{
}

VEngine::Engine::~Engine()
{

}

void VEngine::Engine::start()
{
	m_entityRegistry = std::make_unique<entt::registry>();
	m_window = std::make_unique<Window>(g_windowWidth, g_windowHeight, m_windowTitle);
	m_userInput = std::make_unique<UserInput>();
	m_cameraControllerSystem = std::make_unique<CameraControllerSystem>(*m_entityRegistry, *m_userInput, [=](bool grab) {m_window->grabMouse(grab); });

	// Setup Dear ImGui context
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)m_window->getWindowHandle(), true);
	}

	m_renderSystem = std::make_unique<RenderSystem>(*m_entityRegistry, m_window->getWindowHandle());

	m_window->addInputListener(m_userInput.get());

	m_gameLogic.initialize(this);
	
	Timer timer;
	uint64_t previousTickCount = timer.getElapsedTicks();
	uint64_t frameCount = 0;

	while (!m_shutdown && !m_window->shouldClose())
	{
		timer.update();
		float timeDelta = static_cast<float>(timer.getTimeDelta());

		m_window->pollEvents();

		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

		//ImGui::ShowDemoWindow();

		// gui window
		ImGui::Begin("VEngine");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

		int debugMode = g_giVoxelDebugMode;
		ImGui::RadioButton("None", &debugMode, 0); ImGui::SameLine();
		ImGui::RadioButton("Voxel Scene Raster", &debugMode, 1); ImGui::SameLine();
		ImGui::RadioButton("Voxel Scene Raycast", &debugMode, 2); ImGui::SameLine();
		ImGui::RadioButton("Irradiance Volume", &debugMode, 3);
		ImGui::RadioButton("Irradiance Volume Age", &debugMode, 4);
		g_giVoxelDebugMode = debugMode;
		int cascadeIdx = g_debugVoxelCascadeIndex;
		ImGui::RadioButton("Cascade 0", &cascadeIdx, 0); ImGui::SameLine();
		ImGui::RadioButton("Cascade 1", &cascadeIdx, 1); ImGui::SameLine();
		ImGui::RadioButton("Cascade 2", &cascadeIdx, 2);
		g_debugVoxelCascadeIndex = cascadeIdx;

		const uint32_t *luminancHistogram = m_renderSystem->getLuminanceHistogram();
		float(*histogramValuesGetter)(void* data, int idx) = [](void* data, int idx) -> float
		{
			const uint32_t *luminancHistogram = *(const uint32_t **)data;
			return static_cast<float>(luminancHistogram[idx]);
		};
		ImGui::PlotHistogram("Luminance Histogram", histogramValuesGetter, (void *)&luminancHistogram, RendererConsts::LUMINANCE_HISTOGRAM_SIZE, 0, nullptr, FLT_MAX, FLT_MAX, {0, 80});

		float(*memoryPlotValuesGetter)(void* data, int idx, int *type) = [](void* data, int idx, int *type) -> float
		{
			const VKMemoryBlockDebugInfo *debugInfo = (VKMemoryBlockDebugInfo *)data;
			const auto &span = debugInfo->m_spans[idx];
			*type = (int)span.m_state;
			return static_cast<float>(span.m_offset + span.m_size) / debugInfo->m_allocationSize;
		};

		auto memoryInfo = m_renderSystem->getMemoryAllocatorDebugInfo();

		ImGui::NewLine();
		ImGui::Text("VRAM Allocator");
		ImGui::Separator();
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
		ImGui::Separator();

		ImGui::NewLine();
		ImGui::Text("Profiler");
		ImGui::Separator();
		{
			size_t passTimingCount;
			const PassTimingInfo *passTimingInfo;
			m_renderSystem->getTimingInfo(&passTimingCount, &passTimingInfo);
			float total = 0.0f;
			for (size_t i = 0; i < passTimingCount; ++i)
			{
				ImGui::Text("%30s : %f ms", passTimingInfo[i].m_passName, passTimingInfo[i].m_passTimeWithSync);
				total += passTimingInfo[i].m_passTimeWithSync;
			}
			const char *totalStr = "Total";
			ImGui::Text("%30s : %f ms", totalStr, total);
		}

		ImGui::NewLine();
		ImGui::Text("Occlusion Culling");
		ImGui::Separator();
		{
			uint32_t draws = 0;
			uint32_t totalDraws = 1;
			m_renderSystem->getOcclusionCullingStats(draws, totalDraws);
			ImGui::Text("Draws: %4u | Culled Draws: %4u (%6.2f %%) | Total Draws %4u", draws, totalDraws - draws, float(totalDraws - draws) / totalDraws * 100.0f, totalDraws);
		}
		

		ImGui::End();

		m_userInput->input();
		m_cameraControllerSystem->update(timeDelta);
		m_gameLogic.update(timeDelta);
		ImGui::Render();
		m_renderSystem->update(timeDelta);

		double timeDiff = (timer.getElapsedTicks() - previousTickCount) / static_cast<double>(timer.getTickFrequency());
		if (timeDiff > 1.0)
		{
			double fps = frameCount / timeDiff;
			m_window->setTitle(m_windowTitle + " - " + std::to_string(fps) + " FPS " + std::to_string(1.0 / fps * 1000.0) + " ms");
			previousTickCount = timer.getElapsedTicks();
			frameCount = 0;
		}
		++frameCount;
	}
}

void VEngine::Engine::shutdown()
{
	m_shutdown = true;
}

entt::registry & VEngine::Engine::getEntityRegistry()
{
	return *m_entityRegistry;
}

VEngine::UserInput &VEngine::Engine::getUserInput()
{
	return *m_userInput;
}

VEngine::CameraControllerSystem &VEngine::Engine::getCameraControllerSystem()
{
	return *m_cameraControllerSystem;
}

VEngine::RenderSystem &VEngine::Engine::getRenderSystem()
{
	return *m_renderSystem;
}
