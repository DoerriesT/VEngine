#include "FPSCameraController.h"
#include "Camera.h"
#include "Input/UserInput.h"
#include "Window/Window.h"

VEngine::FPSCameraController::FPSCameraController(UserInput & userInput, Window & window, Camera * camera, float smoothFactor)
	:m_camera(camera),
	m_userInput(userInput),
	m_window(window),
	m_grabbedMouse(false),
	m_smoothFactor(smoothFactor)
{
}

void VEngine::FPSCameraController::setCamera(Camera *camera)
{
	m_camera = camera;
}

void VEngine::FPSCameraController::input(double currentTime, double timeDelta)
{
	if (m_camera)
	{
		bool pressed = false;
		float mod = 1.0f;
		glm::vec3 cameraTranslation;

		glm::vec2 mouseDelta = {};

		if (m_userInput.isMouseButtonPressed(InputMouse::BUTTON_RIGHT))
		{
			if (!m_grabbedMouse)
			{
				//mouseHistory = glm::vec2(0.0f);
				m_grabbedMouse = true;
				m_window.grabMouse(m_grabbedMouse);
			}
			mouseDelta = m_userInput.getMousePosDelta();
		}
		else
		{
			if (m_grabbedMouse)
			{
				m_grabbedMouse = false;
				m_window.grabMouse(m_grabbedMouse);
			}
		}

		m_mouseHistory = glm::mix(mouseDelta, m_mouseHistory, m_smoothFactor);
		if (glm::dot(m_mouseHistory, m_mouseHistory) > 0.0f)
		{
			m_camera->rotate(glm::vec3(m_mouseHistory.y * 0.005f, m_mouseHistory.x * 0.005f, 0.0));
		}


		if (m_userInput.isKeyPressed(InputKey::UP))
		{
			m_camera->rotate(glm::vec3(-timeDelta, 0.0f, 0.0));
		}
		if (m_userInput.isKeyPressed(InputKey::DOWN))
		{
			m_camera->rotate(glm::vec3(timeDelta, 0.0f, 0.0));
		}
		if (m_userInput.isKeyPressed(InputKey::LEFT))
		{
			m_camera->rotate(glm::vec3(0.0f, -timeDelta, 0.0));
		}
		if (m_userInput.isKeyPressed(InputKey::RIGHT))
		{
			m_camera->rotate(glm::vec3(0.0f, timeDelta, 0.0));
		}

		if (m_userInput.isKeyPressed(InputKey::LEFT_SHIFT))
		{
			mod = 5.0f;
		}
		if (m_userInput.isKeyPressed(InputKey::W))
		{
			cameraTranslation.z = -mod * (float)timeDelta;
			pressed = true;
		}
		if (m_userInput.isKeyPressed(InputKey::S))
		{
			cameraTranslation.z = mod * (float)timeDelta;
			pressed = true;
		}
		if (m_userInput.isKeyPressed(InputKey::A))
		{
			cameraTranslation.x = -mod * (float)timeDelta;
			pressed = true;
		}
		if (m_userInput.isKeyPressed(InputKey::D))
		{
			cameraTranslation.x = mod * (float)timeDelta;
			pressed = true;
		}
		if (pressed)
		{
			m_camera->translate(cameraTranslation);
		}
	}
}

void VEngine::FPSCameraController::setSmoothFactor(float smoothFactor)
{
	m_smoothFactor = smoothFactor;
}

float VEngine::FPSCameraController::getSmoothFactor() const
{
	return m_smoothFactor;
}
