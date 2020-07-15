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

		Mobility m_mobility = Mobility::DYNAMIC;
		glm::vec3 m_position = glm::vec3(0.0f);
		glm::quat m_orientation = glm::quat();
		glm::vec3 m_scale = glm::vec3(1.0f);
		glm::mat4 m_transformation;
		glm::mat4 m_previousTransformation;
	};
}