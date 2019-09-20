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
			for (auto &span : info.m_spans)
			{
				freeUsedWasted[(size_t)span.m_state] += span.m_size;
			}
			float freeUsedWastedMB[3];
			float total = 0.0f;
			for (size_t i = 0; i < 3; ++i)
			{
				freeUsedWastedMB[i] = freeUsedWasted[i] * (1.0f / (1024.0f * 1024.0f));
				total += freeUsedWastedMB[i];
			}
			float toPercent = 100.0f * 1.0f / total;
			ImGui::Text("Free: %.3f MB (%.2f %%) | Used: %.3f MB (%.2f %%) | Total: %.3f MB", freeUsedWastedMB[0], freeUsedWastedMB[0] * toPercent, freeUsedWastedMB[1], freeUsedWastedMB[1] * toPercent, total);
		}
		ImGui::Separator();

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
