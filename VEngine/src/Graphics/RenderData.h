#pragma once
#include <cstdint>
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct ViewRenderList;

	struct CommonRenderData
	{
		float m_time;
		float m_fovy;
		float m_nearPlane;
		float m_farPlane;
		glm::mat4 m_viewMatrix;
		glm::mat4 m_projectionMatrix;
		glm::mat4 m_viewProjectionMatrix;
		glm::mat4 m_invViewMatrix;
		glm::mat4 m_invProjectionMatrix;
		glm::mat4 m_invViewProjectionMatrix;
		glm::mat4 m_jitteredProjectionMatrix;
		glm::mat4 m_jitteredViewProjectionMatrix;
		glm::mat4 m_invJitteredProjectionMatrix;
		glm::mat4 m_invJitteredViewProjectionMatrix;
		glm::mat4 m_prevViewMatrix;
		glm::mat4 m_prevProjectionMatrix;
		glm::mat4 m_prevViewProjectionMatrix;
		glm::mat4 m_prevInvViewMatrix;
		glm::mat4 m_prevInvProjectionMatrix;
		glm::mat4 m_prevInvViewProjectionMatrix;
		glm::mat4 m_prevJitteredProjectionMatrix;
		glm::mat4 m_prevJitteredViewProjectionMatrix;
		glm::mat4 m_prevInvJitteredProjectionMatrix;
		glm::mat4 m_prevInvJitteredViewProjectionMatrix;
		glm::vec4 m_cameraPosition;
		glm::vec4 m_cameraDirection;
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_frame;
		uint32_t m_directionalLightCount;
		uint32_t m_directionalLightShadowedCount;
		uint32_t m_directionalLightShadowedProbeCount;
		uint32_t m_punctualLightCount;
		uint32_t m_punctualLightShadowedCount;
		uint32_t m_globalParticipatingMediaCount;
		uint32_t m_localParticipatingMediaCount;
		uint32_t m_curResIdx;
		uint32_t m_prevResIdx;
		float m_timeDelta;
	};

	struct MaterialData
	{
		uint32_t m_albedoOpacity;
		uint32_t m_emissive;
		uint32_t m_metalnessRoughness;
		uint32_t m_albedoNormalTexture;
		uint32_t m_metalnessRoughnessTexture;
		uint32_t m_occlusionEmissiveTexture;
		uint32_t m_displacementTexture;
		uint32_t padding;
	};

	struct SubMeshInfo
	{
		uint32_t m_indexCount;
		uint32_t m_firstIndex;
		int32_t m_vertexOffset;
	};

	struct SubMeshInstanceData
	{
		uint32_t m_subMeshIndex;
		uint32_t m_transformIndex;
		uint32_t m_materialIndex;
	};

	struct RenderData
	{
		uint32_t m_transformDataCount;
		glm::mat4 *m_transformData;
		uint32_t m_shadowMatrixCount;
		glm::mat4 *m_shadowMatrices;
		glm::vec4 *m_shadowCascadeParams;
		glm::mat4 *m_probeViewProjectionMatrices;
		uint32_t m_probeRenderCount;
		uint32_t *m_probeRenderIndices;
		uint32_t m_probeRelightCount;
		uint32_t *m_probeRelightIndices;
		uint32_t m_subMeshInstanceDataCount;
		SubMeshInstanceData *m_subMeshInstanceData;
		uint64_t *m_drawCallKeys;
		uint32_t m_mainViewRenderListIndex;
		uint32_t m_probeDrawListOffset;
		uint32_t m_shadowCascadeViewRenderListOffset;
		uint32_t m_shadowCascadeViewRenderListCount;
		uint32_t m_probeShadowViewRenderListOffset;
		uint32_t m_probeShadowViewRenderListCount;
		ViewRenderList *m_renderLists;
	};
}