#pragma once
#include "VKImage.h"
#include "VKBuffer.h"
#include <memory>
#include "Graphics/RendererConsts.h"
#include "VKMappableBufferBlock.h"

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

		// layouts
		VkImageLayout m_shadowTextureLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout m_taaHistoryTextureLayouts[RendererConsts::FRAMES_IN_FLIGHT] = {};
		VkImageLayout m_gtaoHistoryTextureLayouts[RendererConsts::FRAMES_IN_FLIGHT] = {};
		VkImageLayout m_swapChainImageLayouts[RendererConsts::FRAMES_IN_FLIGHT + 1] = {};

		// samplers
		VkSampler m_shadowSampler;
		VkSampler m_linearSamplerClamp;
		VkSampler m_linearSamplerRepeat;
		VkSampler m_pointSamplerClamp;
		VkSampler m_pointSamplerRepeat;

		// semaphores
		VkSemaphore m_shadowTextureSemaphores[RendererConsts::FRAMES_IN_FLIGHT];
		VkSemaphore m_swapChainImageAvailableSemaphores[RendererConsts::FRAMES_IN_FLIGHT];
		VkSemaphore m_swapChainRenderFinishedSemaphores[RendererConsts::FRAMES_IN_FLIGHT];

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
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);
		void updateTextureArray(const VkDescriptorImageInfo *data, size_t count);
	};
}