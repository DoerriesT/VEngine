#pragma once
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct CameraComponent
	{
		enum class ControllerType
		{
			FPS
		};

		ControllerType m_controllerType = ControllerType::FPS;
		float m_aspectRatio = 1.0f;
		float m_fovy = 1.57f; // 90°
		float m_near = 0.1f;
		float m_far = 300.0f;
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;
	};
}