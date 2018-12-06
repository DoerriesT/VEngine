#pragma once
#include "VKContext.h"
#include <memory>

namespace VEngine
{
	struct VKRenderResources;
	class VKTextureLoader;
	class VKSwapChain;
	class VKGeometryPipeline;
	class VKGeometryAlphaMaskPipeline;
	class VKLightingPipeline;
	class VKForwardPipeline;
	struct RenderParams;
	struct DrawLists;

	class VKRenderer
	{
	public:
		explicit VKRenderer();
		~VKRenderer();
		void init(unsigned int width, unsigned int height);
		void update(const RenderParams &renderParams, const DrawLists &drawLists);
		void render();
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);
		uint32_t loadTexture(const char *filepath);
		void freeTexture(uint32_t id);
		void updateTextureData();

	private:
		unsigned int m_width;
		unsigned int m_height;
		uint32_t m_swapChainImageIndex;
		std::unique_ptr<VKRenderResources> m_renderResources;
		std::unique_ptr<VKTextureLoader> m_textureLoader;
		std::unique_ptr<VKSwapChain> m_swapChain;

		VkRenderPass m_mainRenderPass;
		std::unique_ptr<VKGeometryPipeline> m_geometryPipeline;
		std::unique_ptr<VKGeometryAlphaMaskPipeline> m_geometryAlphaMaskPipeline;
		std::unique_ptr<VKLightingPipeline> m_lightingPipeline;
		std::unique_ptr<VKForwardPipeline> m_forwardPipeline;
	};
}