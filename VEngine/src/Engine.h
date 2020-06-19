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
		uint32_t getWidth() const;
		uint32_t getHeight() const;
		Scene &getScene();
		Window *getWindow();
		void setEditorMode(bool editorMode);
		void setEditorViewport(int32_t offsetX, int32_t offsetY, uint32_t width, uint32_t height);

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
		bool m_editorMode = false;
		int32_t m_editorViewportOffsetX = 0;
		int32_t m_editorViewportOffsetY = 0;
		uint32_t m_editorViewportWidth = 0;
		uint32_t m_editorViewportHeight = 0;
	};
}