#pragma once
#include <memory>
#include <entt/entity/registry.hpp>
#include <vector>
#include "RenderData.h"
#include "LightData.h"
#include "RendererConsts.h"
#include "Handles.h"
#include "Utility/AxisAlignedBoundingBox.h"
#include "BVH.h"
#include "Renderer.h"
#include "ReflectionProbeManager.h"
#include "ParticleEmitterManager.h"

struct ImGuiContext;

namespace VEngine
{
	class Renderer;
	class ReflectionProbeManager;
	struct Material;
	struct SubMesh;
	struct PassTimingInfo;

	class RenderSystem
	{
	public:
		explicit RenderSystem(entt::registry &entityRegistry, void *windowHandle, uint32_t width, uint32_t height);
		void update(float timeDelta);
		Texture2DHandle createTexture(const char *filepath);
		Texture3DHandle createTexture3D(const char *filepath);
		void destroyTexture(Texture2DHandle handle);
		void destroyTexture(Texture3DHandle handle);
		void updateTextureData();
		void updateTexture3DData();
		void createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void destroyMaterials(uint32_t count, MaterialHandle *handles);
		void createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles);
		void destroySubMeshes(uint32_t count, SubMeshHandle *handles);
		entt::entity setCameraEntity(entt::entity cameraEntity);
		entt::entity getCameraEntity() const;
		const uint32_t *getLuminanceHistogram() const;
		//std::vector<VKMemoryBlockDebugInfo> getMemoryAllocatorDebugInfo() const;
		void getTimingInfo(size_t *count, const PassTimingInfo **data) const;
		void getOcclusionCullingStats(uint32_t &draws, uint32_t &totalDraws) const;
		void resize(uint32_t width, uint32_t height);
		void resize(uint32_t editorViewportWidth, uint32_t editorViewportHeight, uint32_t swapChainWidth, uint32_t swapChainHeight);
		void setEditorMode(bool editorMode);
		void initEditorImGuiCtx(ImGuiContext *editorImGuiCtx);
		Texture2DHandle getEditorSceneTextureHandle();

	private:
		entt::registry &m_entityRegistry;
		std::unique_ptr<Renderer> m_renderer;
		std::unique_ptr<ReflectionProbeManager> m_reflectionProbeManager;
		std::unique_ptr<ParticleEmitterManager> m_particleEmitterManager;
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
		uint32_t m_swapChainWidth;
		uint32_t m_swapChainHeight;

		void updateMaterialBatchAssigments(size_t count, const Material *materials, MaterialHandle *handles);
		void calculateCascadeViewProjectionMatrices(const glm::vec3 &lightDir, float maxShadowDistance, float splitLambda, float shadowTextureSize, size_t cascadeCount, glm::mat4 *viewProjectionMatrices, glm::vec4 *cascadeParams);
	};
}