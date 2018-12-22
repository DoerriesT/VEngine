#pragma once
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vector>
#include <array>

namespace VEngine
{
	struct ShadowData
	{
		glm::mat4 m_shadowViewProjectionMatrix;
		glm::vec4 m_shadowCoordScaleBias;
	};

	struct DirectionalLightData
	{
		glm::vec4 m_color;
		glm::vec4 m_direction;
		uint32_t m_shadowDataOffset;
		uint32_t m_shadowDataCount;
	};

	struct PointLightData
	{
		glm::vec4 m_positionRadius;
		glm::vec4 m_colorInvSqrAttRadius;
		//uint32_t m_shadowDataOffset;
		//uint32_t m_shadowDataCount;
	};

	struct SpotLightData
	{
		glm::vec4 m_colorInvSqrAttRadius;
		glm::vec4 m_positionAngleScale;
		glm::vec4 m_directionAngleOffset;
		glm::vec4 m_boundingSphere;
		//uint32_t m_shadowDataOffset;
		//uint32_t m_shadowDataCount;
	};

	struct ShadowJob
	{
		glm::mat4 m_shadowViewProjectionMatrix;
		uint32_t m_offsetX;
		uint32_t m_offsetY;
		uint32_t m_size;
	};

	struct LightData
	{
		std::vector<ShadowData> m_shadowData;
		std::vector<DirectionalLightData> m_directionalLightData;
		std::vector<PointLightData> m_pointLightData;
		std::vector<SpotLightData> m_spotLightData;
		std::vector<ShadowJob> m_shadowJobs;
		std::array<glm::uvec2, 8192> m_zBins;
	};
}