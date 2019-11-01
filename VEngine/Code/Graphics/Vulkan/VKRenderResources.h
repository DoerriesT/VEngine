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

	struct VKRenderResources
	{
		// images
		VKImage m_depthImages[RendererConsts::FRAMES_IN_FLIGHT];
		VKImage m_taaHistoryTextures[RendererConsts::FRAMES_IN_FLIGHT];
		VKImage m_gtaoHistoryTextures[RendererConsts::FRAMES_IN_FLIGHT];
		VKImage m_depthPyramidImages[RendererConsts::FRAMES_IN_FLIGHT];
		VKImage m_imGuiFontsTexture;
		VKImage m_dummyImage;
		VKImage m_voxelSceneImage;

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
		VKBuffer m_stagingBuffer;
		VKBuffer m_materialBuffer;
		VKBuffer m_vertexBuffer;
		VKBuffer m_indexBuffer;
		VKBuffer m_subMeshDataInfoBuffer;
		VKBuffer m_subMeshBoundingBoxBuffer;
		VKBuffer m_uboBuffers[RendererConsts::FRAMES_IN_FLIGHT];
		VKBuffer m_ssboBuffers[RendererConsts::FRAMES_IN_FLIGHT];

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
	};
}