#include "Engine.h"
#include "Window/Window.h"
#include "Input/UserInput.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "Graphics/Vulkan/VKContext.h"
#include "IGameLogic.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include "Graphics/Camera/Camera.h"
#include "Graphics/Camera/FPSCameraController.h"
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#define WINDOW_WIDTH (800)
#define WINDOW_HEIGHT (600)

extern VEngine::VKContext g_context;

VEngine::Engine::Engine(const char * title, IGameLogic & gameLogic)
	:m_gameLogic(gameLogic),
	m_window(std::make_unique<Window>(WINDOW_WIDTH, WINDOW_HEIGHT, title)),
	m_userInput(std::make_unique<UserInput>()),
	m_renderer(std::make_unique<VKRenderer>()),
	m_camera(std::make_unique<Camera>(glm::vec3(), glm::quat(), WINDOW_WIDTH /(float) WINDOW_HEIGHT, glm::radians(60.0f), 0.1f, 300.0f)),
	m_cameraController(std::make_unique<FPSCameraController>(*m_userInput, *m_window, &*m_camera)),
	m_time(),
	m_timeDelta(),
	m_fps(),
	m_lastFpsMeasure(),
	m_shutdown(false)
{
}

VEngine::Engine::~Engine()
{
	
}

void VEngine::Engine::start()
{
	m_window->init();
	m_window->addInputListener(m_userInput.get());
	g_context.init(static_cast<GLFWwindow *>(m_window->getWindowHandle()));
	m_renderer->init(WINDOW_WIDTH, WINDOW_HEIGHT);
	gameLoop();
}

void VEngine::Engine::shutdown()
{
	m_shutdown = true;
}

void VEngine::Engine::gameLoop()
{
	m_lastFpsMeasure = m_time = glfwGetTime();
	while (!m_shutdown && !m_window->shouldClose())
	{
		double time = glfwGetTime();
		m_timeDelta = time - m_time;
		m_time = time;

		input();
		update();
		render();
	}
}

void VEngine::Engine::input()
{
	m_window->pollEvents();
	m_userInput->input();
	m_gameLogic.input(m_time, m_timeDelta);
	m_cameraController->input(m_time, m_timeDelta);
}

void VEngine::Engine::update()
{
	m_renderer->update(m_camera.get());
	m_gameLogic.update(m_time, m_timeDelta);
}

void VEngine::Engine::render()
{
	m_renderer->render();
	m_gameLogic.render();
}
