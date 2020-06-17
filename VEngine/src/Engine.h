#pragma once
#include <memory>
#include <string>
#include <entt/entity/registry.hpp>
#include "Scene.h"

namespace VEngine
{
	class IGameLogic;
	class UserInput;
	class Window;
	class RenderSystem;
	class CameraControllerSystem;

	class Engine
	{
	public:
		explicit Engine(const char *title, IGameLogic &gameLogic);
		Engine(const Engine &) = delete;
		Engine(const Engine &&) = delete;
		Engine &operator= (const Engine &) = delete;
		Engine &operator= (const Engine &&) = delete;
		~Engine();
		void start();
		void shutdown();
		entt::registry &getEntityRegistry();
		UserInput &getUserInput();
		CameraControllerSystem &getCameraControllerSystem();
		RenderSystem &getRenderSystem();
		uint32_t getWindowWidth() const;
		uint32_t getWindowHeight() const;
		Scene &getScene();

	private:
		IGameLogic &m_gameLogic;
		std::unique_ptr<entt::registry> m_entityRegistry;
		std::unique_ptr<Window> m_window;
		std::unique_ptr<UserInput> m_userInput;
		std::unique_ptr<CameraControllerSystem> m_cameraControllerSystem;
		std::unique_ptr<RenderSystem> m_renderSystem;
		bool m_shutdown;
		std::string m_windowTitle;
		Scene m_scene = {};
	};
}