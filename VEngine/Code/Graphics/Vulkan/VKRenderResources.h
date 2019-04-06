#pragma once
#include "VKImage.h"
#include "VKBuffer.h"
#include <memory>
#include "Graphics/RendererConsts.h"

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
			MATERIAL_DATA_BINDING = 1,
			TRANSFORM_DATA_BINDING = 2,
			SHADOW_DATA_BINDING = 3,
			DIRECTIONAL_LIGHT_DATA_BINDING = 4,
			POINT_LIGHT_DATA_BINDING = 5,
			POINT_LIGHT_Z_BINS_BINDING = 6,
			POINT_LIGHT_BITMASK_BINDING = 7,
			PERSISTENT_VALUES_BINDING = 8,
			LUMINANCE_HISTOGRAM_BINDING = 9,
			TEXTURES_BINDING = 10,
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

	namespace TAAResolveSetBindings
	{
		enum
		{
			RESULT_IMAGE_BINDING = 0,
			DEPTH_TEXTURE_BINDING = 1,
			VELOCITY_TEXTURE_BINDING = 2,
			HISTORY_IMAGE_BINDING = 3,
			SOURCE_TEXTURE_BINDING = 4,
		};
	}

	namespace VelocityCompositionSetBindings
	{
		enum
		{
			VELOCITY_IMAGE_BINDING = 0,
			DEPTH_IMAGE_BINDING = 1,
		};
	}

	namespace VelocityInitializationSetBindings
	{
		enum
		{
			DEPTH_IMAGE_BINDING = 0,
		};
	}

	enum
	{
		COMMON_SET_INDEX = 0,
		LIGHTING_SET_INDEX = 1,
		LUMINANCE_HISTOGRAM_SET_INDEX = 2,
		TONEMAP_SET_INDEX = 3,
		TAA_RESOLVE_SET_INDEX = 4,
		VELOCITY_COMPOSITION_SET_INDEX = 5,
		VELOCITY_INITIALIZATION_SET_INDEX,
		MAX_DESCRIPTOR_SET_INDEX = VELOCITY_INITIALIZATION_SET_INDEX
	};
	

	struct VKRenderResources
	{
		// images
		VKImage m_shadowTexture;
		VKImage m_taaHistoryTextures[RendererConsts::FRAMES_IN_FLIGHT];

		// views
		VkImageView m_shadowTextureView;
		VkImageView m_taaHistoryTextureViews[RendererConsts::FRAMES_IN_FLIGHT];

		// buffers
		VKBuffer m_vertexBuffer;
		VKBuffer m_indexBuffer;
		VKBuffer m_lightProxyVertexBuffer;
		VKBuffer m_lightProxyIndexBuffer;
		VKBuffer m_avgLuminanceBuffer;
		VKBuffer m_stagingBuffer;
		VKBuffer m_materialBuffer;

		// layouts
		VkImageLayout m_shadowTextureLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout m_taaHistoryTextureLayouts[RendererConsts::FRAMES_IN_FLIGHT] = {};
		VkImageLayout m_swapChainImageLayouts[RendererConsts::FRAMES_IN_FLIGHT + 1] = {};

		// descriptors
		VkDescriptorPool m_descriptorPool;
		VkDescriptorSetLayout m_descriptorSetLayouts[MAX_DESCRIPTOR_SET_INDEX + 1];
		VkDescriptorSet m_descriptorSets[RendererConsts::FRAMES_IN_FLIGHT][MAX_DESCRIPTOR_SET_INDEX + 1];

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

		std::unique_ptr<VKSyncPrimitiveAllocator> m_syncPrimitiveAllocator;
		std::unique_ptr<VKPipelineManager> m_pipelineManager;

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