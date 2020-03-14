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

		ControllerType m_controllerType;
		float m_aspectRatio;
		float m_fovy;
		float m_near;
		float m_far;
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;
	};
}