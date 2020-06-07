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

	struct PunctualLight
	{
		glm::vec3 m_color;
		float m_invSqrAttRadius;
		glm::vec3 m_position;
		float m_angleScale;
		glm::vec3 m_direction;
		float m_angleOffset;
	};

	struct PunctualLightShadowed
	{
		PunctualLight m_light;
		glm::vec4 m_shadowMatrix0;
		glm::vec4 m_shadowMatrix1;
		glm::vec4 m_shadowMatrix2;
		glm::vec4 m_shadowMatrix3;
		glm::vec4 m_shadowAtlasParams[6]; // x: scale, y: biasX, z: biasY, w: unused
		glm::vec4 m_fomShadowAtlasParams; // x: scale, y: biasX, z: biasY, w: volumetric shadows enabled
		glm::vec3 m_positionWS;
		float m_radius;
	};

	struct ShadowAtlasDrawInfo
	{
		uint32_t m_shadowMatrixIdx;
		uint32_t m_drawListIdx;
		uint32_t m_offsetX;
		uint32_t m_offsetY;
		uint32_t m_size;
	};

	struct FOMAtlasDrawInfo
	{
		glm::mat4 m_shadowMatrix;
		glm::vec3 m_lightPosition;
		float m_lightRadius;
		uint32_t m_offsetX;
		uint32_t m_offsetY;
		uint32_t m_size;
		bool m_pointLight;
	};

	struct GlobalParticipatingMedium
	{
		glm::vec3 m_emissive;
		float m_extinction;
		glm::vec3 m_scattering;
		float m_phase;
		uint32_t m_heightFogEnabled;
		float m_heightFogStart;
		float m_heightFogFalloff;
		float m_maxHeight;
		glm::vec3 m_noiseBias;
		float m_noiseScale;
		float m_noiseIntensity;
		float m_pad0;
		float m_pad1;
		float m_pad2;
	};

	struct LocalParticipatingMedium
	{
		glm::vec4 m_worldToLocal0;
		glm::vec4 m_worldToLocal1;
		glm::vec4 m_worldToLocal2;
		glm::vec3 m_position;
		float m_phase;
		glm::vec3 m_emissive;
		float m_extinction;
		glm::vec3 m_scattering;
		float m_noiseIntensity;
		glm::vec3 m_noiseScale;
		float m_heightFogStart;
		glm::vec3 m_noiseBias;
		float m_heightFogFalloff;
	};

	struct LocalReflectionProbe
	{
		glm::vec4 m_worldToLocal0;
		glm::vec4 m_worldToLocal1;
		glm::vec4 m_worldToLocal2;
		glm::vec3 m_capturePosition;
		float m_arraySlot;
	};

	struct ReflectionProbeRelightData
	{
		glm::vec3 m_position;
		float m_nearPlane;
		float m_farPlane;
	};

	struct LightData
	{
		std::vector<ShadowAtlasDrawInfo> m_shadowAtlasDrawInfos;
		std::vector<FOMAtlasDrawInfo> m_fomAtlasDrawInfos;;
		std::vector<DirectionalLight> m_directionalLights;
		std::vector<DirectionalLight> m_directionalLightsShadowed;
		std::vector<DirectionalLight> m_directionalLightsShadowedProbe;
		std::vector<PunctualLight> m_punctualLights;
		std::vector<PunctualLightShadowed> m_punctualLightsShadowed;
		std::vector<GlobalParticipatingMedium> m_globalParticipatingMedia;
		std::vector<LocalParticipatingMedium> m_localParticipatingMedia;
		std::vector<LocalReflectionProbe> m_localReflectionProbes;
		std::vector<glm::mat4> m_punctualLightTransforms;
		std::vector<glm::mat4> m_punctualLightShadowedTransforms;
		std::vector<glm::mat4> m_localMediaTransforms;
		std::vector<glm::mat4> m_localReflectionProbeTransforms;
		std::vector<uint32_t> m_punctualLightOrder;
		std::vector<uint32_t> m_punctualLightShadowedOrder;
		std::vector<uint32_t> m_localMediaOrder;
		std::vector<uint32_t> m_localReflectionProbeOrder;
		std::array<uint32_t, RendererConsts::Z_BINS> m_punctualLightDepthBins;
		std::array<uint32_t, RendererConsts::Z_BINS> m_punctualLightShadowedDepthBins;
		std::array<uint32_t, RendererConsts::Z_BINS> m_localMediaDepthBins;
		std::array<uint32_t, RendererConsts::Z_BINS> m_localReflectionProbeDepthBins;
		const ReflectionProbeRelightData *m_reflectionProbeRelightData;

		void clear()
		{
			m_shadowAtlasDrawInfos.clear();
			m_fomAtlasDrawInfos.clear();
			m_directionalLights.clear();
			m_directionalLightsShadowed.clear();
			m_directionalLightsShadowedProbe.clear();
			m_punctualLights.clear();
			m_punctualLightsShadowed.clear();
			m_globalParticipatingMedia.clear();
			m_localParticipatingMedia.clear();
			m_localReflectionProbes.clear();
			m_punctualLightTransforms.clear();
			m_punctualLightShadowedTransforms.clear();
			m_localMediaTransforms.clear();
			m_localReflectionProbeTransforms.clear();
			m_punctualLightOrder.clear();
			m_punctualLightShadowedOrder.clear();
			m_localMediaOrder.clear();
			m_localReflectionProbeOrder.clear();
			memset(m_punctualLightDepthBins.data(), 0, sizeof(m_punctualLightDepthBins));
			memset(m_punctualLightShadowedDepthBins.data(), 0, sizeof(m_punctualLightShadowedDepthBins));
			memset(m_localMediaDepthBins.data(), 0, sizeof(m_localMediaDepthBins));
			memset(m_localMediaDepthBins.data(), 0, sizeof(m_localReflectionProbeDepthBins));
		}
	};
}