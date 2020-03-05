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
#include "Graphics/GALTestRenderer.h"

float g_ssrBias = 0.0f;

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
	m_window = std::make_unique<Window>(1600, 900, m_windowTitle);
	uint32_t width = m_window->getWidth();
	uint32_t height = m_window->getHeight();
	m_userInput = std::make_unique<UserInput>();
	m_cameraControllerSystem = std::make_unique<CameraControllerSystem>(*m_entityRegistry, *m_userInput, [=](bool grab) {m_window->grabMouse(grab); });

	// Setup Dear ImGui context
	//{
	//	IMGUI_CHECKVERSION();
	//	ImGui::CreateContext();
	//	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	//	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	//
	//	io.ConfigDockingWithShift = true;
	//
	//	// Setup Dear ImGui style
	//	ImGui::StyleColorsDark();
	//	//ImGui::StyleColorsClassic();
	//
	//	// Setup Platform/Renderer bindings
	//	ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)m_window->getWindowHandle(), true);
	//}

	//m_renderSystem = std::make_unique<RenderSystem>(*m_entityRegistry, m_window->getWindowHandle(), width, height);

	m_window->addInputListener(m_userInput.get());

	m_gameLogic.initialize(this);
	
	Timer timer;
	uint64_t previousTickCount = timer.getElapsedTicks();
	uint64_t frameCount = 0;
	Editor editor(this, width, height);
	
	//m_renderSystem->setCameraEntity(editor.getEditorCameraEntity());

	GALTestRenderer testRenderer(width, height, m_window->getWindowHandle());

	while (!m_shutdown && !m_window->shouldClose())
	{
		timer.update();
		float timeDelta = static_cast<float>(timer.getTimeDelta());

		m_window->pollEvents();
		if (m_window->configurationChanged())
		{
			width = m_window->getWidth();
			height = m_window->getHeight();
			//m_renderSystem->resize(width, height);
			editor.resize(width, height);
		}

		//ImGui_ImplGlfw_NewFrame();
		//ImGui::NewFrame();
		//ImGuizmo::BeginFrame();

		m_userInput->input();
		m_cameraControllerSystem->update(timeDelta);
		m_gameLogic.update(timeDelta);
		//editor.update(timeDelta);

		//ImGui::ShowDemoWindow();

		//ImGui::Render();
		//m_renderSystem->update(timeDelta);
		testRenderer.render();

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
