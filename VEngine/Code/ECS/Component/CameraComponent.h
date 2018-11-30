#pragma once
#include "Component.h"
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

namespace VEngine
{
	struct CameraComponent : public Component<CameraComponent>
	{
		float m_aspectRatio;
		float m_fovy;
		float m_near;
		float m_far;
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;

		explicit CameraComponent(float aspectRatio, float fovy, float nearPlane, float farPlane)
			:m_aspectRatio(aspectRatio),
			m_fovy(fovy),
			m_near(nearPlane),
			m_far(farPlane),
			m_viewMatrix(),
			m_projectionMatrix()
		{
		}
	};
}