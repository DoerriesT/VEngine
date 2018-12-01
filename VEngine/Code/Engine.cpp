#include "Engine.h"
#include "Window/Window.h"
#include "Input/UserInput.h"
#include "Graphics/Vulkan/VKRenderer.h"
#include "Graphics/Vulkan/VKContext.h"
#include "IGameLogic.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include "ECS/EntityManager.h"
#include "ECS/SystemManager.h"
#include "ECS/System/RenderSystem.h"
#include "ECS/System/CameraControllerSystem.h"
#include "GlobalVar.h"

extern VEngine::VKContext g_context;
namespace VEngine
{
	extern GlobalVar<unsigned int> g_windowWidth;
	extern GlobalVar<unsigned int> g_windowHeight;
}


VEngine::Engine::Engine(const char * title, IGameLogic & gameLogic)
	:m_gameLogic(gameLogic),
	m_window(std::make_unique<Window>(g_windowWidth, g_windowHeight, title)),
	m_userInput(std::make_unique<UserInput>()),
	m_entityManager(std::make_unique<EntityManager>()),
	m_systemManager(std::make_unique<SystemManager>()),
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
	m_systemManager->addSystem<CameraControllerSystem>(*m_entityManager, *m_userInput, [this](bool grab) {m_window->grabMouse(grab); });
	m_systemManager->addSystem<RenderSystem>(*m_entityManager);
	m_systemManager->init();
	m_gameLogic.init();
	gameLoop();
}

void VEngine::Engine::shutdown()
{
	m_shutdown = true;
}

VEngine::EntityManager & VEngine::Engine::getEntityManager()
{
	return *m_entityManager;
}

VEngine::SystemManager & VEngine::Engine::getSystemManager()
{
	return *m_systemManager;
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
	m_systemManager->input(m_time, m_timeDelta);
	m_gameLogic.input(m_time, m_timeDelta);
}

void VEngine::Engine::update()
{
	m_systemManager->update(m_time, m_timeDelta);
	m_gameLogic.update(m_time, m_timeDelta);
}

void VEngine::Engine::render()
{
	m_systemManager->render();
	m_gameLogic.render();
}
