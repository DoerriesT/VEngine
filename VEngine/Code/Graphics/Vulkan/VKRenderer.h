#pragma once
#include "VKContext.h"
#include <memory>

namespace VEngine
{
	namespace FrameGraph
	{
		class Graph;
	}
	struct VKRenderResources;
	class VKTextureLoader;
	class VKSwapChain;
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

	private:
		unsigned int m_width;
		unsigned int m_height;
		uint32_t m_swapChainImageIndex;
		uint32_t m_fontAtlasTextureIndex;

		std::unique_ptr<FrameGraph::Graph> m_frameGraphs[2];
		std::unique_ptr<VKRenderResources> m_renderResources;
		std::unique_ptr<VKTextureLoader> m_textureLoader;
		std::unique_ptr<VKSwapChain> m_swapChain;
	};
}