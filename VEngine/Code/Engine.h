#pragma once
#include <memory>

namespace VEngine
{
	class IGameLogic;
	class UserInput;
	class Window;
	class VKRenderer;
	class Camera;
	class FPSCameraController;

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

	private:
		IGameLogic &m_gameLogic;
		std::unique_ptr<Window> m_window;
		std::unique_ptr<UserInput> m_userInput;
		std::unique_ptr<VKRenderer> m_renderer;
		std::unique_ptr<Camera> m_camera;
		std::unique_ptr<FPSCameraController> m_cameraController;
		double m_time;
		double m_timeDelta;
		double m_fps;
		double m_lastFpsMeasure;
		bool m_shutdown;
		
		void gameLoop();
		void input();
		void update();
		void render();
	};
}