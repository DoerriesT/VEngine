#include "Engine.h"
#include "Window/Window.h"
#include "Input/UserInput.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "Graphics/Vulkan/VKContext.h"
#include "IGameLogic.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include "Graphics/RenderSystem.h"
#include "Input/CameraControllerSystem.h"
#include "GlobalVar.h"

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
	m_renderSystem = std::make_unique<RenderSystem>(*m_entityRegistry, m_window->getWindowHandle());

	m_window->addInputListener(m_userInput.get());

	m_gameLogic.initialize(this);
	
	m_lastFpsMeasure = m_time = glfwGetTime();
	while (!m_shutdown && !m_window->shouldClose())
	{
		double time = glfwGetTime();
		m_timeDelta = time - m_time;
		m_time = time;

		m_window->pollEvents();
		m_userInput->input();
		m_cameraControllerSystem->update(m_timeDelta);
		m_gameLogic.update(m_timeDelta);
		m_renderSystem->update(m_timeDelta);

		double difference = m_time - m_lastFpsMeasure;
		if (difference > 1.0)
		{
			m_fps /= difference;
			m_window->setTitle(m_windowTitle + " - " + std::to_string(m_fps) + " FPS " + std::to_string(1.0 / m_fps * 1000.0) + " ms");
			m_lastFpsMeasure = m_time;
			m_fps = 0.0;
		}
		++m_fps;
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
