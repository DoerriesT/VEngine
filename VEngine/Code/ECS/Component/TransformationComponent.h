#pragma once
#include "Component.h"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace VEngine
{
	struct TransformationComponent : public Component<TransformationComponent>
	{
		static const std::uint64_t FAMILY_ID;
		Mobility m_mobility;
		glm::vec3 m_position;
		glm::quat m_rotation;
		glm::vec3 m_scale;
		glm::mat4 m_transformation;
		glm::mat4 m_prevTransformation;

		explicit TransformationComponent(Mobility _mobility, const glm::vec3 &_position = glm::vec3(), const glm::quat &_rotation = glm::quat(), const glm::vec3 &_scale = glm::vec3(1.0f))
			:m_mobility(_mobility), 
			m_position(_position), 
			m_rotation(_rotation), 
			m_scale(_scale) 
		{ 
		}
	};
}