#pragma once
#include <memory>
#include <entt/entity/registry.hpp>
#include "RenderParams.h"
#include "DrawItem.h"
#include "LightData.h"
#include "RendererConsts.h"

namespace VEngine
{
	class VKRenderer;

	class RenderSystem
	{
	public:
		explicit RenderSystem(entt::registry &entityRegistry, void *windowHandle);
		void update(float timeDelta);
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);
		uint32_t createTexture(const char *filepath);
		void updateTextureData();
		void setCameraEntity(entt::entity cameraEntity);
		entt::entity getCameraEntity() const;

	private:
		entt::registry &m_entityRegistry;
		std::unique_ptr<VKRenderer> m_renderer;
		entt::entity m_cameraEntity;
		RenderParams m_renderParams;
		DrawLists m_drawLists;
		LightData m_lightData;
		float m_haltonX[RendererConsts::MAX_TAA_HALTON_SAMPLES];
		float m_haltonY[RendererConsts::MAX_TAA_HALTON_SAMPLES];

		void calculateCascadeViewProjectionMatrices(const RenderParams &renderParams, const glm::vec3 &lightDir, float nearPlane, float farPlane, float splitLambda, float shadowTextureSize, size_t cascadeCount, glm::mat4 *viewProjectionMatrices);
	};
}