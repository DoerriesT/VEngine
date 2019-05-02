#pragma once
#include <memory>
#include <entt/entity/registry.hpp>
#include <vector>
#include "RenderData.h"
#include "LightData.h"
#include "RendererConsts.h"
#include "Handles.h"
#include "Utility/AxisAlignedBoundingBox.h"

namespace VEngine
{
	class VKRenderer;
	struct Material;

	class RenderSystem
	{
	public:
		explicit RenderSystem(entt::registry &entityRegistry, void *windowHandle);
		void update(float timeDelta);
		TextureHandle createTexture(const char *filepath);
		void destroyTexture(TextureHandle handle);
		void updateTextureData();
		void createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void destroyMaterials(uint32_t count, MaterialHandle *handles);
		void createSubMeshes(uint32_t count, uint32_t *vertexSizes, const uint8_t *const*vertexData, uint32_t *indexCounts, const uint32_t *const*indexData, AxisAlignedBoundingBox *aabbs, SubMeshHandle *handles);
		void destroySubMeshes(uint32_t count, SubMeshHandle *handles);
		void setCameraEntity(entt::entity cameraEntity);
		entt::entity getCameraEntity() const;

	private:
		entt::registry &m_entityRegistry;
		std::unique_ptr<VKRenderer> m_renderer;
		entt::entity m_cameraEntity;
		std::unique_ptr<uint8_t[]> m_materialBatchAssignment;
		std::unique_ptr<AxisAlignedBoundingBox[]> m_aabbs;
		CommonRenderData m_commonRenderData;
		std::vector<glm::mat4> m_transformData;
		std::vector<SubMeshInstanceData> m_opaqueSubMeshInstanceData;
		std::vector<SubMeshInstanceData> m_maskedSubMeshInstanceData;
		LightData m_lightData;
		float m_haltonX[RendererConsts::MAX_TAA_HALTON_SAMPLES];
		float m_haltonY[RendererConsts::MAX_TAA_HALTON_SAMPLES];

		void calculateCascadeViewProjectionMatrices(const CommonRenderData &renderParams, const glm::vec3 &lightDir, float nearPlane, float farPlane, float splitLambda, float shadowTextureSize, size_t cascadeCount, glm::mat4 *viewProjectionMatrices);
		void updateMaterialBatchAssigments(size_t count, const Material *materials, MaterialHandle *handles);
	};
}