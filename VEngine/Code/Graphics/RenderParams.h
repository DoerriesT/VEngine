#pragma once
#include <cstdint>
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct RenderParams
	{
		float m_time;
		float m_fovy;
		float m_nearPlane;
		float m_farPlane;
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;
		glm::mat4 m_viewProjectionMatrix;
		glm::mat4 m_invViewMatrix;
		glm::mat4 m_invProjectionMatrix;
		glm::mat4 m_invViewProjectionMatrix;
		glm::mat4 m_prevViewMatrix;
		glm::mat4 m_prevProjectionMatrix;
		glm::mat4 m_prevViewProjectionMatrix;
		glm::mat4 m_prevInvViewMatrix;
		glm::mat4 m_prevInvProjectionMatrix;
		glm::mat4 m_prevInvViewProjectionMatrix;
		glm::vec4 m_cameraPosition;
		glm::vec4 m_cameraDirection;
		uint32_t m_frame;
	};
}