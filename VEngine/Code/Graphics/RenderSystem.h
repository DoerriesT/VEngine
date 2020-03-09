#pragma once
#include <memory>
#include <entt/entity/registry.hpp>
#include <vector>
#include "RenderData.h"
#include "LightData.h"
#include "RendererConsts.h"
#include "Handles.h"
#include "Utility/AxisAlignedBoundingBox.h"
#include "Vulkan/VKMemoryAllocator.h"
#include "BVH.h"
#include "Graphics/Renderer.h"

namespace VEngine
{
	class Renderer;
	struct Material;
	struct SubMesh;
	struct PassTimingInfo;

	class RenderSystem
	{
	public:
		explicit RenderSystem(entt::registry &entityRegistry, void *windowHandle, uint32_t width, uint32_t height);
		void update(float timeDelta);
		TextureHandle createTexture(const char *filepath);
		void destroyTexture(TextureHandle handle);
		void updateTextureData();
		void createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void destroyMaterials(uint32_t count, MaterialHandle *handles);
		void createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles);
		void destroySubMeshes(uint32_t count, SubMeshHandle *handles);
		void setCameraEntity(entt::entity cameraEntity);
		entt::entity getCameraEntity() const;
		const uint32_t *getLuminanceHistogram() const;
		std::vector<VKMemoryBlockDebugInfo> getMemoryAllocatorDebugInfo() const;
		void getTimingInfo(size_t *count, const PassTimingInfo **data) const;
		void getOcclusionCullingStats(uint32_t &draws, uint32_t &totalDraws) const;
		void resize(uint32_t width, uint32_t height);

	private:
		entt::registry &m_entityRegistry;
		std::unique_ptr<Renderer> m_renderer;
		entt::entity m_cameraEntity;
		std::unique_ptr<uint8_t[]> m_materialBatchAssignment;
		std::unique_ptr<AxisAlignedBoundingBox[]> m_aabbs;
		std::unique_ptr<glm::vec4[]> m_boundingSpheres;
		CommonRenderData m_commonRenderData;
		std::vector<glm::mat4> m_transformData;
		std::vector<glm::mat4> m_shadowMatrices;
		std::vector<glm::vec4> m_shadowCascadeParams;
		std::vector<SubMeshInstanceData> m_subMeshInstanceData;
		LightData m_lightData;
		float m_haltonX[RendererConsts::MAX_TAA_HALTON_SAMPLES];
		float m_haltonY[RendererConsts::MAX_TAA_HALTON_SAMPLES];
		BVH m_bvh;
		uint32_t m_width;
		uint32_t m_height;

		void updateMaterialBatchAssigments(size_t count, const Material *materials, MaterialHandle *handles);
		void calculateCascadeViewProjectionMatrices(const glm::vec3 &lightDir, float maxShadowDistance, float splitLambda, float shadowTextureSize, size_t cascadeCount, glm::mat4 *viewProjectionMatrices, glm::vec4 *cascadeParams);
	};
}