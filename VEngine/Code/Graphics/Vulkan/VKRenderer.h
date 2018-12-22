#pragma once
#include "VKContext.h"
#include <memory>
#include <glm/vec4.hpp>
#include <glm/vec2.hpp>

namespace VEngine
{
	struct VKRenderResources;
	class VKTextureLoader;
	class VKSwapChain;
	class VKShadowRenderPass;
	class VKGeometryRenderPass;
	class VKForwardRenderPass;
	class VKTilingPipeline;
	class VKShadowPipeline;
	class VKGeometryPipeline;
	class VKGeometryAlphaMaskPipeline;
	class VKLightingPipeline;
	class VKForwardPipeline;
	struct RenderParams;
	struct DrawLists;
	struct LightData;

	class VKRenderer
	{
	public:
		explicit VKRenderer();
		~VKRenderer();
		void init(unsigned int width, unsigned int height);
		void render(const RenderParams &renderParams, const DrawLists &drawLists, const LightData &lightData);
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);
		uint32_t loadTexture(const char *filepath);
		void freeTexture(uint32_t id);
		void updateTextureData();
		unsigned char *getDirectionalLightDataPtr(uint32_t &maxLights, size_t &itemSize);
		unsigned char *getPointLightDataPtr(uint32_t &maxLights, size_t &itemSize);
		unsigned char *getSpotLightDataPtr(uint32_t &maxLights, size_t &itemSize);
		unsigned char *getShadowDataPtr(uint32_t &maxShadows, size_t &itemSize);
		glm::uvec2 *getZBinPtr(uint32_t &bins, uint32_t &binDepth);
		glm::vec4 *getPointLightCullDataPtr(uint32_t &maxLights);
		glm::vec4 *getSpotLightCullDataPtr(uint32_t &maxLights);

	private:
		unsigned int m_width;
		unsigned int m_height;
		uint32_t m_swapChainImageIndex;
		std::unique_ptr<VKRenderResources> m_renderResources;
		std::unique_ptr<VKTextureLoader> m_textureLoader;
		std::unique_ptr<VKSwapChain> m_swapChain;

		std::unique_ptr<VKTilingPipeline> m_tilingPipeline;
		std::unique_ptr<VKGeometryRenderPass> m_geometryRenderPass;
		std::unique_ptr<VKShadowRenderPass> m_shadowRenderPass;
		std::unique_ptr<VKForwardRenderPass> m_forwardRenderPass;
		std::unique_ptr<VKShadowPipeline> m_shadowPipeline;
		std::unique_ptr<VKGeometryPipeline> m_geometryPipeline;
		std::unique_ptr<VKGeometryAlphaMaskPipeline> m_geometryAlphaMaskPipeline;
		std::unique_ptr<VKLightingPipeline> m_lightingPipeline;
		std::unique_ptr<VKForwardPipeline> m_forwardPipeline;
	};
}