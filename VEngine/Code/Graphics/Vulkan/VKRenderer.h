#pragma once
#include "VKContext.h"
#include <memory>
#include "Graphics/RendererConsts.h"
#include "Handles.h"

namespace VEngine
{
	namespace FrameGraph
	{
		class Graph;
	}
	struct VKRenderResources;
	class VKTextureLoader;
	class VKSwapChain;
	class VKMaterialManager;
	struct CommonRenderData;
	struct RenderData;
	struct LightData;
	struct Material;

	class VKRenderer
	{
	public:
		explicit VKRenderer(uint32_t width, uint32_t height, void *windowHandle);
		~VKRenderer();
		void render(const CommonRenderData &commonData, const RenderData &renderData, const LightData &lightData);
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);
		TextureHandle loadTexture(const char *filepath);
		void freeTexture(TextureHandle id);
		void createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void destroyMaterials(uint32_t count, MaterialHandle *handles);
		void updateTextureData();

	private:
		std::unique_ptr<VKRenderResources> m_renderResources;
		std::unique_ptr<VKTextureLoader> m_textureLoader;
		std::unique_ptr<VKMaterialManager> m_materialManager;
		std::unique_ptr<VKSwapChain> m_swapChain;
		std::unique_ptr<FrameGraph::Graph> m_frameGraphs[RendererConsts::FRAMES_IN_FLIGHT];

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_swapChainImageIndex;
		TextureHandle m_fontAtlasTextureIndex;
	};
}