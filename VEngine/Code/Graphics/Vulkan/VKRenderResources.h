#pragma once
#include "VKImage.h"
#include "VKBuffer.h"
#include <memory>
#include "Graphics/RendererConsts.h"
#include "VKMappableBufferBlock.h"
#include "RenderGraph.h"

namespace VEngine
{
	class VKRenderer;
	class VKPipelineCache;
	class VKMappableBufferBlock;
	struct BVHNode;
	struct Triangle;

	struct VKRenderResources
	{
		// images
		VKImage m_depthImages[RendererConsts::FRAMES_IN_FLIGHT];
		VKImage m_taaHistoryTextures[RendererConsts::FRAMES_IN_FLIGHT];
		VKImage m_gtaoHistoryTextures[RendererConsts::FRAMES_IN_FLIGHT];
		VKImage m_depthPyramidImages[RendererConsts::FRAMES_IN_FLIGHT];
		VKImage m_imGuiFontsTexture;
		VKImage m_dummyImage;
		VKImage m_brickPointerImage;
		VKImage m_voxelSceneImage;
		VKImage m_voxelSceneOpacityImage;
		VKImage m_irradianceVolumeImage;
		VKImage m_irradianceVolumeDepthImage;
		/*VKImage m_irradianceVolumeXAxisImage;
		VKImage m_irradianceVolumeYAxisImage;
		VKImage m_irradianceVolumeZAxisImage;*/
		VKImage m_irradianceVolumeAgeImage;

		// views
		VkImageView m_imGuiFontsTextureView;
		VkImageView m_dummyImageView;

		// buffers
		VKBuffer m_lightProxyVertexBuffer;
		VKBuffer m_lightProxyIndexBuffer;
		VKBuffer m_boxIndexBuffer;
		VKBuffer m_avgLuminanceBuffer;
		VKBuffer m_luminanceHistogramReadBackBuffers[RendererConsts::FRAMES_IN_FLIGHT];
		VKBuffer m_occlusionCullStatsReadBackBuffers[RendererConsts::FRAMES_IN_FLIGHT];
		VKBuffer m_brickPoolStatsReadBackBuffers[RendererConsts::FRAMES_IN_FLIGHT];
		VKBuffer m_stagingBuffer;
		VKBuffer m_materialBuffer;
		VKBuffer m_vertexBuffer;
		VKBuffer m_indexBuffer;
		VKBuffer m_subMeshDataInfoBuffer;
		VKBuffer m_subMeshBoundingBoxBuffer;
		VKBuffer m_uboBuffers[RendererConsts::FRAMES_IN_FLIGHT];
		VKBuffer m_ssboBuffers[RendererConsts::FRAMES_IN_FLIGHT];
		VKBuffer m_irradianceVolumeQueueBuffers[RendererConsts::FRAMES_IN_FLIGHT];
		VKBuffer m_freeBricksBuffer;
		VKBuffer m_binVisBricksBuffer;
		VKBuffer m_colorBricksBuffer;
		VKBuffer m_bvhNodesBuffer = {};
		VKBuffer m_bvhTrianglesBuffer = {};

		// mappable buffer blocks
		std::unique_ptr<VKMappableBufferBlock> m_mappableUBOBlock[RendererConsts::FRAMES_IN_FLIGHT];
		std::unique_ptr<VKMappableBufferBlock> m_mappableSSBOBlock[RendererConsts::FRAMES_IN_FLIGHT];

		// samplers
		VkSampler m_samplers[4];
		VkSampler m_shadowSampler;

		// semaphores
		VkSemaphore m_swapChainImageAvailableSemaphores[RendererConsts::FRAMES_IN_FLIGHT];
		VkSemaphore m_swapChainRenderFinishedSemaphores[RendererConsts::FRAMES_IN_FLIGHT];

		// rendergraph external info
		VkQueue m_depthImageQueue[RendererConsts::FRAMES_IN_FLIGHT];
		ResourceState m_depthImageResourceState[RendererConsts::FRAMES_IN_FLIGHT];
		VkQueue m_taaHistoryTextureQueue[RendererConsts::FRAMES_IN_FLIGHT];
		ResourceState m_taaHistoryTextureResourceState[RendererConsts::FRAMES_IN_FLIGHT];
		VkQueue m_gtaoHistoryTextureQueue[RendererConsts::FRAMES_IN_FLIGHT];
		ResourceState m_gtaoHistoryTextureResourceState[RendererConsts::FRAMES_IN_FLIGHT];
		VkQueue m_depthPyramidImageQueue[RendererConsts::FRAMES_IN_FLIGHT];
		ResourceState m_depthPyramidImageResourceState[RendererConsts::FRAMES_IN_FLIGHT];
		VkQueue m_avgLuminanceBufferQueue = RenderGraph::undefinedQueue;
		ResourceState m_avgLuminanceBufferResourceState = ResourceState::UNDEFINED;
		VkQueue m_voxelSceneImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_voxelSceneImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_voxelSceneOpacityImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_voxelSceneOpacityImageResourceState = ResourceState::UNDEFINED;
		/*VkQueue m_irradianceVolumeXAxisImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_irradianceVolumeXAxisImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_irradianceVolumeYAxisImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_irradianceVolumeYAxisImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_irradianceVolumeZAxisImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_irradianceVolumeZAxisImageResourceState = ResourceState::UNDEFINED;*/
		VkQueue m_irradianceVolumeAgeImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_irradianceVolumeAgeImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_irradianceVolumeQueueBuffersQueue[RendererConsts::FRAMES_IN_FLIGHT];
		ResourceState m_irradianceVolumeQueueBuffersResourceState[RendererConsts::FRAMES_IN_FLIGHT];
		VkQueue m_irradianceVolumeImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_irradianceVolumeImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_irradianceVolumeDepthImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_irradianceVolumeDepthImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_brickPointerImageQueue = RenderGraph::undefinedQueue;
		ResourceState m_brickPointerImageResourceState = ResourceState::UNDEFINED;
		VkQueue m_freeBricksBufferQueue = RenderGraph::undefinedQueue;
		ResourceState m_freeBricksBufferResourceState = ResourceState::UNDEFINED;
		VkQueue m_colorBricksBufferQueue = RenderGraph::undefinedQueue;
		ResourceState m_colorBricksBufferResourceState = ResourceState::UNDEFINED;
		VkQueue m_binVisBricksBufferQueue = RenderGraph::undefinedQueue;
		ResourceState m_binVisBricksBufferResourceState = ResourceState::UNDEFINED;

		VkDescriptorSetLayout m_textureDescriptorSetLayout;
		VkDescriptorSetLayout m_computeTextureDescriptorSetLayout;
		VkDescriptorSet m_textureDescriptorSet;
		VkDescriptorSet m_computeTextureDescriptorSet;
		VkDescriptorSetLayout m_imGuiDescriptorSetLayout;
		VkDescriptorSet m_imGuiDescriptorSet;
		VkDescriptorPool m_textureDescriptorSetPool;

		explicit VKRenderResources();
		VKRenderResources(const VKRenderResources &) = delete;
		VKRenderResources(const VKRenderResources &&) = delete;
		VKRenderResources &operator= (const VKRenderResources &) = delete;
		VKRenderResources &operator= (const VKRenderResources &&) = delete;
		~VKRenderResources();
		void init(uint32_t width, uint32_t height);
		void resize(uint32_t width, uint32_t height);
		void updateTextureArray(const VkDescriptorImageInfo *data, size_t count);
		void createImGuiFontsTexture();
		void setBVH(uint32_t nodeCount, const BVHNode *nodes, uint32_t triangleCount, const Triangle *triangles);
	};
}