#pragma once
#include <glm/vec2.hpp>

namespace VEngine
{
	class Camera;
	class UserInput;
	class Window;

	class FPSCameraController
	{
	public:
		explicit FPSCameraController(UserInput &userInput, Window &window, Camera *camera = nullptr, float smoothFactor = 0.0f);
		void setCamera(Camera *camera);
		void input(double currentTime, double timeDelta);
		void setSmoothFactor(float smoothFactor);
		float getSmoothFactor() const;

	private:
		Camera *m_camera;
		UserInput &m_userInput;
		Window &m_window;
		bool m_grabbedMouse;
		glm::vec2 m_mouseHistory;
		float m_smoothFactor;
	};
}