#pragma once
#include "Component.h"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace VEngine
{
	struct TransformationComponent : public Component<TransformationComponent>
	{
		enum class Mobility
		{
			STATIC, DYNAMIC
		};

		Mobility m_mobility;
		glm::vec3 m_position;
		glm::quat m_rotation;
		glm::vec3 m_scale;
		glm::mat4 m_transformation;
		glm::mat4 m_prevTransformation;

		explicit TransformationComponent(Mobility mobility, const glm::vec3 &position = glm::vec3(), const glm::quat &rotation = glm::quat(), const glm::vec3 &scale = glm::vec3(1.0f))
			:m_mobility(mobility), 
			m_position(position), 
			m_rotation(rotation), 
			m_scale(scale) 
		{ 
		}
	};
}