#include "Engine.h"
#include "Window/Window.h"
#include "Input/UserInput.h"
#include "IGameLogic.h"
#include "Graphics/RenderSystem.h"
#include "Input/CameraControllerSystem.h"
#include "GlobalVar.h"
#include "Utility/Timer.h"
#include "graphics/imgui/imgui.h"
//#include "graphics/imgui/imgui_impl_glfw.h"
#include "Graphics/imgui/ImGuizmo.h"
#include "Editor/Editor.h"
#include "Input/ImGuiInputAdapter.h"

float g_ssrBias = 0.7f;

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
	m_window = std::make_unique<Window>(1600, 900, Window::WindowMode::WINDOWED, m_windowTitle);
	uint32_t width = m_window->getWidth();
	uint32_t height = m_window->getHeight();
	m_userInput = std::make_unique<UserInput>(*m_window.get());
	m_cameraControllerSystem = std::make_unique<CameraControllerSystem>(*m_entityRegistry, *m_userInput, [=](bool grab) {m_window->setMouseCursorMode(grab ? Window::MouseCursorMode::DISABLED : Window::MouseCursorMode::NORMAL); });

	constexpr size_t FRAME_TIME_ARRAY_SIZE = 64;
	std::unique_ptr<float[]> frametimes = std::make_unique<float[]>(FRAME_TIME_ARRAY_SIZE);

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
		//ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)m_window->getWindowHandle(), true);
	}

	ImGuiInputAdapter imguiInputAdapter(ImGui::GetCurrentContext(), *m_userInput.get(), *m_window.get());
	imguiInputAdapter.resize(m_window->getWidth(), m_window->getHeight(), m_window->getWindowWidth(), m_window->getWindowHeight());

	m_renderSystem = std::make_unique<RenderSystem>(*m_entityRegistry, m_window->getWindowHandle(), width, height);

	m_gameLogic.initialize(this);

	bool vsyncChanged = false;
	g_VSyncEnabled.addListener([&](bool value)
		{
			vsyncChanged = true;
		});
	
	Timer timer;
	uint64_t previousTickCount = timer.getElapsedTicks();
	uint64_t frameCount = 0;
	
	//m_renderSystem->setCameraEntity(editor.getEditorCameraEntity());

	while (!m_shutdown && !m_window->shouldClose())
	{
		timer.update();
		float timeDelta = static_cast<float>(timer.getTimeDelta());

		m_window->pollEvents();
		if (m_window->configurationChanged() || m_viewportParamsDirty || vsyncChanged)
		{
			width = m_window->getWidth();
			height = m_window->getHeight();
			uint32_t windowWidth = m_window->getWindowWidth();
			uint32_t windowHeight = m_window->getWindowHeight();
			int32_t offsetX = 0;
			int32_t offsetY = 0;

			if (m_editorMode)
			{
				width = m_editorViewportWidth;
				height = m_editorViewportHeight;
				windowWidth = m_editorViewportWidth;
				windowHeight = m_editorViewportHeight;
				offsetX = m_editorViewportOffsetX;
				offsetY = m_editorViewportOffsetY;
			}

			m_viewportParamsDirty = false;

			imguiInputAdapter.resize(width, height, windowWidth, windowHeight);
			m_userInput->resize(offsetX, offsetY, windowWidth, windowHeight);
			m_renderSystem->resize(width, height, m_window->getWidth(), m_window->getHeight(), m_window->getWindowMode() == Window::WindowMode::FULL_SCREEN, g_VSyncEnabled);
			
			vsyncChanged = false;
		}

		m_userInput->input();
		//ImGui_ImplGlfw_NewFrame();
		imguiInputAdapter.update();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

		
		m_cameraControllerSystem->update(timeDelta, m_renderSystem->getCameraEntity());
		m_gameLogic.update(timeDelta);

		ImGui::ShowDemoWindow();

		memmove(frametimes.get(), frametimes.get() + 1, (FRAME_TIME_ARRAY_SIZE - 1) * sizeof(float));
		frametimes[FRAME_TIME_ARRAY_SIZE - 1] = timeDelta * 1000.0f;

		// profiler window
		{
			ImGui::Begin("Profiler");
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			char overlay[32];
			sprintf_s(overlay, "%10.10f ms", frametimes[FRAME_TIME_ARRAY_SIZE - 1]);
			ImGui::PlotLines("Frametime", frametimes.get(), FRAME_TIME_ARRAY_SIZE, 0, overlay, 0.0f, 50.0f, ImVec2(0, 80));

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

			ImGui::End();
		}

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

uint32_t VEngine::Engine::getWindowWidth() const
{
	return m_window->getWidth();
}

uint32_t VEngine::Engine::getWindowHeight() const
{
	return m_window->getHeight();
}

uint32_t VEngine::Engine::getWidth() const
{
	return m_editorMode ? m_editorViewportWidth : m_window->getWidth();
}

uint32_t VEngine::Engine::getHeight() const
{
	return m_editorMode ? m_editorViewportHeight : m_window->getHeight();
}

VEngine::Scene &VEngine::Engine::getScene()
{
	return m_scene;
}

VEngine::Window *VEngine::Engine::getWindow()
{
	return m_window.get();
}

void VEngine::Engine::setEditorMode(bool editorMode)
{
	m_editorMode = editorMode;
	m_viewportParamsDirty = true;
	m_renderSystem->setEditorMode(m_editorMode);
}

void VEngine::Engine::setEditorViewport(int32_t offsetX, int32_t offsetY, uint32_t width, uint32_t height)
{
	if (m_editorViewportOffsetX != offsetX || m_editorViewportOffsetY != offsetY || m_editorViewportWidth != width || m_editorViewportHeight != height)
	{
		m_viewportParamsDirty = true;
		m_editorViewportOffsetX = offsetX;
		m_editorViewportOffsetY = offsetY;
		m_editorViewportWidth = width;
		m_editorViewportHeight = height;
	}
}
