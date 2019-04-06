#pragma once
#include <memory>
#include <entt/entity/registry.hpp>
#include <vector>
#include "RenderData.h"
#include "LightData.h"
#include "RendererConsts.h"
#include "Handles.h"

namespace VEngine
{
	class VKRenderer;
	struct Material;

	class RenderSystem
	{
	public:
		explicit RenderSystem(entt::registry &entityRegistry, void *windowHandle);
		void update(float timeDelta);
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);
		uint32_t createTexture(const char *filepath);
		void updateTextureData();
		void createMaterials(size_t count, const Material *materials, MaterialHandle *handles);
		void updateMaterials(size_t count, const Material *materials, MaterialHandle *handles);
		void destroyMaterials(size_t count, MaterialHandle *handles);
		void setCameraEntity(entt::entity cameraEntity);
		entt::entity getCameraEntity() const;

	private:
		entt::registry &m_entityRegistry;
		std::unique_ptr<VKRenderer> m_renderer;
		entt::entity m_cameraEntity;
		std::unique_ptr<uint8_t[]> m_materialBatchAssignment;
		CommonRenderData m_commonRenderData;
		std::vector<glm::mat4> m_transformData;
		std::vector<SubMeshData> m_subMeshData;
		std::vector<SubMeshInstanceData> m_opaqueSubMeshInstanceData;
		std::vector<SubMeshInstanceData> m_maskedSubMeshInstanceData;
		LightData m_lightData;
		float m_haltonX[RendererConsts::MAX_TAA_HALTON_SAMPLES];
		float m_haltonY[RendererConsts::MAX_TAA_HALTON_SAMPLES];

		void calculateCascadeViewProjectionMatrices(const CommonRenderData &renderParams, const glm::vec3 &lightDir, float nearPlane, float farPlane, float splitLambda, float shadowTextureSize, size_t cascadeCount, glm::mat4 *viewProjectionMatrices);
		void updateMaterialBatchAssigments(size_t count, const Material *materials, MaterialHandle *handles);
	};
}