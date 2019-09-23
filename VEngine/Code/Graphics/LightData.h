#pragma once
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vector>
#include <array>
#include "RendererConsts.h"

namespace VEngine
{
	struct DirectionalLightData
	{
		glm::vec4 m_color;
		glm::vec4 m_direction;
		uint32_t m_shadowMatricesOffsetCount;
	};

	struct PointLightData
	{
		glm::vec4 m_positionRadius;
		glm::vec4 m_colorInvSqrAttRadius;
	};

	struct SpotLightData
	{
		glm::vec4 m_colorInvSqrAttRadius;
		glm::vec4 m_positionAngleScale;
		glm::vec4 m_directionAngleOffset;
		glm::vec4 m_boundingSphere;
	};

	struct LightData
	{
		std::vector<DirectionalLightData> m_directionalLightData;
		std::vector<PointLightData> m_pointLightData;
		std::vector<SpotLightData> m_spotLightData;
		std::array<uint32_t, RendererConsts::Z_BINS> m_zBins;
	};
}