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
#include "Editor/Editor.h"

uint32_t g_debugVoxelCascadeIndex = 0;
uint32_t g_giVoxelDebugMode = 0;
uint32_t g_allocatedBricks = 0;

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
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		io.ConfigDockingWithShift = true;

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
	Editor editor(this);
	
	m_renderSystem->setCameraEntity(editor.getEditorCameraEntity());

	while (!m_shutdown && !m_window->shouldClose())
	{
		timer.update();
		float timeDelta = static_cast<float>(timer.getTimeDelta());

		m_window->pollEvents();

		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

		m_userInput->input();
		m_cameraControllerSystem->update(timeDelta);
		m_gameLogic.update(timeDelta);
		editor.update();

		//ImGui::ShowDemoWindow();


		// gui window
		ImGui::Begin("VEngine");

		bool casEnabled = g_CASEnabled;
		ImGui::Checkbox("CAS", &casEnabled);
		g_CASEnabled = casEnabled;
		float casSharpness = g_CASSharpness;
		ImGui::SliderFloat("CAS Sharpness", &casSharpness, 0.0f, 1.0f);
		g_CASSharpness = casSharpness;

		ImGui::NewLine();
		ImGui::Text("Allocated Bricks: %d (%6.2f %%)", g_allocatedBricks, float(g_allocatedBricks) / (1024.0f * 64.0f) * 100.0f);
		
		int debugMode = g_giVoxelDebugMode;
		ImGui::RadioButton("None", &debugMode, 0); ImGui::SameLine();
		ImGui::RadioButton("Voxel Scene Raster", &debugMode, 1); ImGui::SameLine();
		ImGui::RadioButton("Voxel Scene Raycast", &debugMode, 2); ImGui::SameLine();
		ImGui::RadioButton("Irradiance Volume", &debugMode, 3);
		ImGui::RadioButton("Irradiance Volume Age", &debugMode, 4);
		ImGui::RadioButton("Bricks", &debugMode, 6);
		g_giVoxelDebugMode = debugMode;
		int cascadeIdx = g_debugVoxelCascadeIndex;
		ImGui::RadioButton("Cascade 0", &cascadeIdx, 0); ImGui::SameLine();
		ImGui::RadioButton("Cascade 1", &cascadeIdx, 1); ImGui::SameLine();
		ImGui::RadioButton("Cascade 2", &cascadeIdx, 2);
		g_debugVoxelCascadeIndex = cascadeIdx;
		
		ImGui::NewLine();
		ImGui::Text("Occlusion Culling");
		ImGui::Separator();
		{
			uint32_t draws = 0;
			uint32_t totalDraws = 1;
			m_renderSystem->getOcclusionCullingStats(draws, totalDraws);
			ImGui::Text("Total Probes: %4u | Culled Probes: %4u (%6.2f %%)", 64 * 64 * 32 * 3, draws, float(draws) / (64 * 64 * 32 * 3) * 100.0f);
			//ImGui::Text("Draws: %4u | Culled Draws: %4u (%6.2f %%) | Total Draws %4u", draws, totalDraws - draws, float(totalDraws - draws) / totalDraws * 100.0f, totalDraws);
		}
		
		ImGui::End();

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
