#pragma once
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <vector>
#include <array>
#include "RendererConsts.h"

namespace VEngine
{
	struct DirectionalLight
	{
		glm::vec3 m_color;
		uint32_t m_shadowOffset;
		glm::vec3 m_direction;
		uint32_t m_shadowCount;
	};

	struct PointLight
	{
		glm::vec3 m_color;
		float m_invSqrAttRadius;
		glm::vec3 m_position;
		float m_radius;
	};

	struct PointLightShadowed
	{
		PointLight m_pointLight;
		glm::vec4 m_shadowAtlasParams[6];
	};

	struct SpotLight
	{
		glm::vec3 m_color;
		float m_invSqrAttRadius;
		glm::vec3 m_position;
		float m_angleScale;
		glm::vec3 m_direction;
		float m_angleOffset;
	};

	struct SpotLightShadowed
	{
		SpotLight m_spotLight;
		glm::vec3 m_shadowAtlasScaleBias;
		uint32_t m_shadowOffset;
	};

	struct ShadowAtlasDrawInfo
	{
		uint32_t m_shadowMatrixIdx;
		uint32_t m_drawListIdx;
		uint32_t m_offsetX;
		uint32_t m_offsetY;
		uint32_t m_size;
	};

	struct LightData
	{
		std::vector<ShadowAtlasDrawInfo> m_shadowAtlasDrawInfos;
		std::vector<DirectionalLight> m_directionalLights;
		std::vector<PointLight> m_pointLights;
		std::vector<SpotLight> m_spotLights;
		std::vector<PointLightShadowed> m_pointLightsShadowed;
		std::vector<DirectionalLight> m_directionalLightsShadowed;
		std::vector<SpotLightShadowed> m_spotLightsShadowed;
		std::vector<glm::mat4> m_pointLightTransforms;
		std::vector<glm::mat4> m_spotLightTransforms;
		std::vector<glm::mat4> m_spotLightShadowedTransforms;
		std::vector<uint32_t> m_pointLightOrder;
		std::vector<uint32_t> m_spotLightOrder;
		std::vector<uint32_t> m_spotLightShadowedOrder;
		std::array<uint32_t, RendererConsts::Z_BINS> m_pointLightDepthBins;
		std::array<uint32_t, RendererConsts::Z_BINS> m_spotLightDepthBins;
		std::array<uint32_t, RendererConsts::Z_BINS> m_spotLightShadowedDepthBins;

		void clear()
		{
			m_shadowAtlasDrawInfos.clear();
			m_directionalLights.clear();
			m_pointLights.clear();
			m_spotLights.clear();
			m_pointLightsShadowed.clear();
			m_directionalLightsShadowed.clear();
			m_spotLightsShadowed.clear();
			m_pointLightTransforms.clear();
			m_spotLightTransforms.clear();
			m_spotLightShadowedTransforms.clear();
			m_pointLightOrder.clear();
			m_spotLightOrder.clear();
			m_spotLightShadowedOrder.clear();
			memset(m_pointLightDepthBins.data(), 0, sizeof(m_pointLightDepthBins));
			memset(m_spotLightDepthBins.data(), 0, sizeof(m_spotLightDepthBins));
			memset(m_spotLightShadowedDepthBins.data(), 0, sizeof(m_spotLightShadowedDepthBins));
		}
	};
}