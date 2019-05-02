#pragma once
#include <cstdint>
#include <glm/mat4x4.hpp>

namespace VEngine
{
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
		uint32_t m_pointLightCount;
		uint32_t m_currentResourceIndex;
		uint32_t m_previousResourceIndex;
		float m_timeDelta;
	};

	struct alignas(16) MaterialData
	{
		float m_albedoFactor[3];
		float m_metalnessFactor;
		float m_emissiveFactor[3];
		float m_roughnessFactor;
		uint32_t m_albedoNormalTexture;
		uint32_t m_metalnessRoughnessTexture;
		uint32_t m_occlusionEmissiveTexture;
		uint32_t m_displacementTexture;
	};

	struct SubMeshData
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
		uint32_t m_opaqueSubMeshInstanceDataCount;
		SubMeshInstanceData *m_opaqueSubMeshInstanceData;
		uint32_t m_maskedSubMeshInstanceDataCount;
		SubMeshInstanceData *m_maskedSubMeshInstanceData;
	};
}