#pragma once
#include "Component.h"
#include <glm/vec3.hpp>

namespace VEngine
{
	struct ShadowMapTile;

	struct DirectionalLightComponent : public Component<DirectionalLightComponent>
	{
		glm::vec3 m_color;
		glm::vec3 m_direction;
		const ShadowMapTile *m_shadowMapTiles[4];

		explicit DirectionalLightComponent(const glm::vec3 &color, const glm::vec3 &direction)
			:m_color(color),
			m_direction(direction),
			m_shadowMapTiles()
		{
		}
	};
}