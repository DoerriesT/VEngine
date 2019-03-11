#pragma once
#include "VKImage.h"
#include "VKBuffer.h"
#include <vector>
#include <memory>

#define FRAMES_IN_FLIGHT (2)
#define MAX_UNIFORM_BUFFER_INSTANCE_COUNT (512)
#define TEXTURE_ARRAY_SIZE (512)
#define MAX_DIRECTIONAL_LIGHTS (1)
#define MAX_POINT_LIGHTS (65536)
#define MAX_SPOT_LIGHTS (65536)
#define MAX_SHADOW_DATA (512)
#define Z_BINS (8192)

namespace VEngine
{
	class VKRenderer;
	class VKSyncPrimitiveAllocator;
	class VKPipelineManager;

	struct VKRenderResources
	{
		// images
		VKImage m_shadowTexture;

		// views
		VkImageView m_shadowTextureView;

		// buffers
		VKBuffer m_vertexBuffer;
		VKBuffer m_indexBuffer;

		// layouts
		VkImageLayout m_shadowTextureLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout m_swapChainImageLayouts[3] = {};

		// descriptors
		VkDescriptorPool m_descriptorPool;
		VkDescriptorSetLayout m_perFrameDataDescriptorSetLayout;
		VkDescriptorSetLayout m_perDrawDataDescriptorSetLayout;
		VkDescriptorSetLayout m_textureDescriptorSetLayout;
		VkDescriptorSetLayout m_lightingInputDescriptorSetLayout;
		VkDescriptorSetLayout m_lightDataDescriptorSetLayout;
		VkDescriptorSetLayout m_cullDataDescriptorSetLayout;
		VkDescriptorSetLayout m_lightBitMaskDescriptorSetLayout;
		VkDescriptorSet m_perFrameDataDescriptorSets[FRAMES_IN_FLIGHT];
		VkDescriptorSet m_perDrawDataDescriptorSets[FRAMES_IN_FLIGHT];
		VkDescriptorSet m_textureDescriptorSet;
		VkDescriptorSet m_lightingInputDescriptorSets[FRAMES_IN_FLIGHT];
		VkDescriptorSet m_lightDataDescriptorSets[FRAMES_IN_FLIGHT];
		VkDescriptorSet m_cullDataDescriptorSets[FRAMES_IN_FLIGHT];
		VkDescriptorSet m_lightBitMaskDescriptorSets[FRAMES_IN_FLIGHT];

		// samplers
		VkSampler m_shadowSampler;
		VkSampler m_linearSampler;
		VkSampler m_pointSampler;

		// semaphores
		VkSemaphore m_shadowTextureSemaphores[FRAMES_IN_FLIGHT];
		VkSemaphore m_swapChainImageAvailableSemaphores[FRAMES_IN_FLIGHT];
		VkSemaphore m_swapChainRenderFinishedSemaphores[FRAMES_IN_FLIGHT];

		std::unique_ptr<VKSyncPrimitiveAllocator> m_syncPrimitiveAllocator;
		std::unique_ptr<VKPipelineManager> m_pipelineManager;

		explicit VKRenderResources();
		VKRenderResources(const VKRenderResources &) = delete;
		VKRenderResources(const VKRenderResources &&) = delete;
		VKRenderResources &operator= (const VKRenderResources &) = delete;
		VKRenderResources &operator= (const VKRenderResources &&) = delete;
		~VKRenderResources();
		void init(unsigned int width, unsigned int height, VkFormat swapchainFormat);
		void resize(unsigned int width, unsigned int height);
		void reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize);
		void uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize);
		void updateTextureArray(const VkDescriptorImageInfo *data, size_t count);
	};
}