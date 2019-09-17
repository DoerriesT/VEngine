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
		VKImage m_shadowTexture;
		VKImage m_taaHistoryTextures[RendererConsts::FRAMES_IN_FLIGHT];
		VKImage m_gtaoHistoryTextures[RendererConsts::FRAMES_IN_FLIGHT];

		// views
		VkImageView m_shadowTextureView;
		VkImageView m_taaHistoryTextureViews[RendererConsts::FRAMES_IN_FLIGHT];
		VkImageView m_gtaoHistoryTextureViews[RendererConsts::FRAMES_IN_FLIGHT];

		// buffers
		VKBuffer m_lightProxyVertexBuffer;
		VKBuffer m_lightProxyIndexBuffer;
		VKBuffer m_avgLuminanceBuffer;
		VKBuffer m_stagingBuffer;
		VKBuffer m_materialBuffer;
		VKBuffer m_vertexBuffer;
		VKBuffer m_indexBuffer;
		VKBuffer m_subMeshDataInfoBuffer;
		VKBuffer m_uboBuffers[RendererConsts::FRAMES_IN_FLIGHT];
		VKBuffer m_ssboBuffers[RendererConsts::FRAMES_IN_FLIGHT];

		// mappable buffer blocks
		std::unique_ptr<VKMappableBufferBlock> m_mappableUBOBlock[RendererConsts::FRAMES_IN_FLIGHT];
		std::unique_ptr<VKMappableBufferBlock> m_mappableSSBOBlock[RendererConsts::FRAMES_IN_FLIGHT];

		// samplers
		VkSampler m_shadowSampler;
		VkSampler m_linearSamplerClamp;
		VkSampler m_linearSamplerRepeat;
		VkSampler m_pointSamplerClamp;
		VkSampler m_pointSamplerRepeat;

		// semaphores
		VkSemaphore m_swapChainImageAvailableSemaphores[RendererConsts::FRAMES_IN_FLIGHT];
		VkSemaphore m_swapChainRenderFinishedSemaphores[RendererConsts::FRAMES_IN_FLIGHT];

		// rendergraph external info
		VkQueue m_shadowMapQueue = RenderGraph::undefinedQueue;
		ResourceState m_shadowMapResourceState = ResourceState::UNDEFINED;
		VkQueue m_taaHistoryTextureQueue[RendererConsts::FRAMES_IN_FLIGHT];
		ResourceState m_taaHistoryTextureResourceState[RendererConsts::FRAMES_IN_FLIGHT];
		VkQueue m_gtaoHistoryTextureQueue[RendererConsts::FRAMES_IN_FLIGHT];
		ResourceState m_gtaoHistoryTextureResourceState[RendererConsts::FRAMES_IN_FLIGHT];
		VkQueue m_avgLuminanceBufferQueue = RenderGraph::undefinedQueue;
		ResourceState m_avgLuminanceBufferResourceState = ResourceState::UNDEFINED;

		VkDescriptorSetLayout m_textureDescriptorSetLayout;
		VkDescriptorSet m_textureDescriptorSet;
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
	};
}