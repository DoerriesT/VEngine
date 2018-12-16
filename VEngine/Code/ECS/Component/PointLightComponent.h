#pragma once
#include "Component.h"
#include <glm/vec3.hpp>

namespace VEngine
{
	struct ShadowMapTile;

	struct PointLightComponent : public Component<PointLightComponent>
	{
		glm::vec3 m_color;
		float m_luminousPower;
		float m_radius;
		const ShadowMapTile *m_shadowMapTiles[6];

		explicit PointLightComponent(const glm::vec3 &color, float luminousPower, float radius)
			:m_color(color),
			m_luminousPower(luminousPower),
			m_radius(radius),
			m_shadowMapTiles()
		{
		}
	};
}