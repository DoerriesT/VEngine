#pragma once
#include "VKImage.h"
#include "VKBuffer.h"
#include <vector>
#include <memory>

#define FRAMES_IN_FLIGHT (2)
#define TEXTURE_ARRAY_SIZE (512)
#define MAX_DIRECTIONAL_LIGHTS (1)
#define MAX_POINT_LIGHTS (65536)
#define MAX_SPOT_LIGHTS (65536)
#define MAX_SHADOW_DATA (512)
#define Z_BINS (8192)
#define TILE_SIZE (16)
#define LUMINANCE_HISTOGRAM_SIZE (256)

namespace VEngine
{
	class VKRenderer;
	class VKSyncPrimitiveAllocator;
	class VKPipelineManager;

	namespace CommonSetBindings
	{
		enum
		{
			PER_FRAME_DATA_BINDING = 0,
			PER_DRAW_DATA_BINDING = 1,
			SHADOW_DATA_BINDING = 2,
			DIRECTIONAL_LIGHT_DATA_BINDING = 3,
			POINT_LIGHT_DATA_BINDING = 4,
			POINT_LIGHT_Z_BINS_BINDING = 5,
			POINT_LIGHT_BITMASK_BINDING = 6,
			PERSISTENT_VALUES_BINDING = 7,
			LUMINANCE_HISTOGRAM_BINDING = 8,
			TEXTURES_BINDING = 9,
		};
	}
	
	namespace LightingSetBindings
	{
		enum
		{
			RESULT_IMAGE_BINDING = 0,
			G_BUFFER_TEXTURES_BINDING = 1,
			SHADOW_TEXTURE_BINDING = 2,
		};
	}
	
	namespace LuminanceHistogramSetBindings
	{
		enum
		{
			SOURCE_TEXTURE_BINDING = 0
		};
	}
	
	namespace TonemapSetBindings
	{
		enum
		{
			RESULT_IMAGE_BINDING = 0,
			SOURCE_TEXTURE_BINDING = 1
		};
	}

	enum
	{
		COMMON_SET_INDEX = 0,
		LIGHTING_SET_INDEX = 1,
		LUMINANCE_HISTOGRAM_SET_INDEX = 2,
		TONEMAP_SET_INDEX = 3,
		MAX_DESCRIPTOR_SET_INDEX = TONEMAP_SET_INDEX
	};
	

	struct VKRenderResources
	{
		// images
		VKImage m_shadowTexture;

		// views
		VkImageView m_shadowTextureView;

		// buffers
		VKBuffer m_vertexBuffer;
		VKBuffer m_indexBuffer;
		VKBuffer m_lightProxyVertexBuffer;
		VKBuffer m_lightProxyIndexBuffer;
		VKBuffer m_avgLuminanceBuffer;

		// layouts
		VkImageLayout m_shadowTextureLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout m_swapChainImageLayouts[3] = {};

		// descriptors
		VkDescriptorPool m_descriptorPool;
		VkDescriptorSetLayout m_descriptorSetLayouts[MAX_DESCRIPTOR_SET_INDEX + 1];
		VkDescriptorSet m_descriptorSets[FRAMES_IN_FLIGHT][MAX_DESCRIPTOR_SET_INDEX + 1];

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