#pragma once
#include <memory>
#include <string>

namespace VEngine
{
	class IGameLogic;
	class UserInput;
	class Window;
	class EntityManager;
	class SystemManager;

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
		EntityManager &getEntityManager();
		SystemManager &getSystemManager();
		UserInput &getUserInput();

	private:
		IGameLogic &m_gameLogic;
		std::unique_ptr<Window> m_window;
		std::unique_ptr<UserInput> m_userInput;
		std::unique_ptr<EntityManager> m_entityManager;
		std::unique_ptr<SystemManager> m_systemManager;
		double m_time;
		double m_timeDelta;
		double m_fps;
		double m_lastFpsMeasure;
		bool m_shutdown;
		std::string m_windowTitle;
		
		void gameLoop();
		void input();
		void update();
		void render();
	};
}