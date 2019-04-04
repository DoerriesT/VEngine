#pragma once
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct TransformationComponent
	{
		enum Mobility
		{
			STATIC, DYNAMIC
		};

		Mobility m_mobility;
		glm::vec3 m_position;
		glm::quat m_orientation;
		float m_scale = 1.0f;
		glm::mat4 m_transformation;
		glm::mat4 m_previousTransformation;
	};
}