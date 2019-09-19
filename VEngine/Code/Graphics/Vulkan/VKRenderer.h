#pragma once
#include "VKContext.h"
#include <memory>
#include "Graphics/RendererConsts.h"
#include "Handles.h"

namespace VEngine
{
	class RenderGraph;
	struct VKRenderResources;
	class VKTextureLoader;
	class VKSwapChain;
	class VKMaterialManager;
	class VKMeshManager;
	class VKPipelineCache;
	class VKDescriptorSetCache;
	class DeferredObjectDeleter;
	class RenderPassCache;
	struct CommonRenderData;
	struct RenderData;
	struct LightData;
	struct Material;
	struct SubMesh;

	class VKRenderer
	{
	public:
		explicit VKRenderer(uint32_t width, uint32_t height, void *windowHandle);
		~VKRenderer();
		void render(const CommonRenderData &commonData, const RenderData &renderData, const LightData &lightData);
		TextureHandle loadTexture(const char *filepath);
		void freeTexture(TextureHandle id);
		void createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles);
		void destroyMaterials(uint32_t count, MaterialHandle *handles);
		void createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles);
		void destroySubMeshes(uint32_t count, SubMeshHandle *handles);
		void updateTextureData();
		const uint32_t *getLuminanceHistogram() const;

	private:
		std::unique_ptr<VKRenderResources> m_renderResources;
		std::unique_ptr<DeferredObjectDeleter> m_deferredObjectDeleter;
		std::unique_ptr<VKPipelineCache> m_pipelineCache;
		std::unique_ptr<RenderPassCache> m_renderPassCache;
		std::unique_ptr<VKDescriptorSetCache> m_descriptorSetCache;
		std::unique_ptr<VKTextureLoader> m_textureLoader;
		std::unique_ptr<VKMaterialManager> m_materialManager;
		std::unique_ptr<VKMeshManager> m_meshManager;
		std::unique_ptr<VKSwapChain> m_swapChain;
		std::unique_ptr<RenderGraph> m_frameGraphs[RendererConsts::FRAMES_IN_FLIGHT];

		uint32_t m_luminanceHistogram[RendererConsts::LUMINANCE_HISTOGRAM_SIZE];

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_swapChainImageIndex;
		TextureHandle m_fontAtlasTextureIndex;
	};
}