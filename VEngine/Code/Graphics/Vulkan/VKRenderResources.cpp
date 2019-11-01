#include "VKRenderResources.h"
#include "VKUtility.h"
#include "VKContext.h"
#include "Utility/Utility.h"
#include "GlobalVar.h"
#include "VKPipelineCache.h"
#include "Graphics/RenderData.h"
#include "Graphics/Mesh.h"
#include "RenderGraph.h"
#include "Graphics/imgui/imgui.h"

VEngine::VKRenderResources::~VKRenderResources()
{

}

void VEngine::VKRenderResources::init(uint32_t width, uint32_t height)
{
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_depthImageQueue[i] = RenderGraph::undefinedQueue;
		m_depthImageResourceState[i] = ResourceState::UNDEFINED;
		m_taaHistoryTextureQueue[i] = RenderGraph::undefinedQueue;
		m_taaHistoryTextureResourceState[i] = ResourceState::UNDEFINED;
		m_gtaoHistoryTextureQueue[i] = RenderGraph::undefinedQueue;
		m_gtaoHistoryTextureResourceState[i] = ResourceState::UNDEFINED;
		m_swapChainImageAvailableSemaphores[i] = g_context.m_syncPrimitivePool.acquireSemaphore();
		m_swapChainRenderFinishedSemaphores[i] = g_context.m_syncPrimitivePool.acquireSemaphore();
	}

	uint32_t queueFamilyIndices[] =
	{
		g_context.m_queueFamilyIndices.m_graphicsFamily,
		g_context.m_queueFamilyIndices.m_computeFamily,
		g_context.m_queueFamilyIndices.m_transferFamily
	};

	// depth images
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
		imageCreateInfo.extent.width = width;
		imageCreateInfo.extent.height = height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			m_depthImages[i].create(imageCreateInfo, allocCreateInfo);
		}
	}

	// TAA history textures
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageCreateInfo.extent.width = width;
		imageCreateInfo.extent.height = height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			m_taaHistoryTextures[i].create(imageCreateInfo, allocCreateInfo);
		}
	}

	// GTAO history textures
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R16G16_SFLOAT;
		imageCreateInfo.extent.width = width;
		imageCreateInfo.extent.height = height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			m_gtaoHistoryTextures[i].create(imageCreateInfo, allocCreateInfo);
		}
	}

	// dummy image
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCreateInfo.extent.width = 1;
		imageCreateInfo.extent.height = 1;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_dummyImage.create(imageCreateInfo, allocCreateInfo);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = 1;

		VkCommandBuffer copyCmd = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		{
			VKUtility::setImageLayout(
				copyCmd,
				m_dummyImage.getImage(),
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				subresourceRange,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, copyCmd);

		VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = m_dummyImage.getImage();
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_dummyImage.getFormat();
		viewInfo.subresourceRange = subresourceRange;

		if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_dummyImageView) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image view!", -1);
		}
	}

	// voxel scene image
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageCreateInfo.extent.width = RendererConsts::VOXEL_SCENE_WIDTH;
		imageCreateInfo.extent.height = RendererConsts::VOXEL_SCENE_DEPTH; // width and depth are same size
		imageCreateInfo.extent.depth = RendererConsts::VOXEL_SCENE_HEIGHT * RendererConsts::VOXEL_SCENE_CASCADES; // keep all cascades in same image by extending image depth
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_voxelSceneImage.create(imageCreateInfo, allocCreateInfo);
	}

	// irradiance volume images
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageCreateInfo.extent.width = RendererConsts::IRRADIANCE_VOLUME_WIDTH;
		imageCreateInfo.extent.height = RendererConsts::IRRADIANCE_VOLUME_DEPTH; // width and depth are same size
		imageCreateInfo.extent.depth = RendererConsts::IRRADIANCE_VOLUME_HEIGHT * 2 // both positive and negative direction is in same image
			* RendererConsts::IRRADIANCE_VOLUME_CASCADES; // keep all cascades in same image by extending image depth
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_irradianceVolumeXAxisImage.create(imageCreateInfo, allocCreateInfo);
		m_irradianceVolumeYAxisImage.create(imageCreateInfo, allocCreateInfo);
		m_irradianceVolumeZAxisImage.create(imageCreateInfo, allocCreateInfo);
	}

	// mappable blocks
	{
		// ubo
		{
			VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bufferCreateInfo.size = RendererConsts::MAPPABLE_UBO_BLOCK_SIZE;
			bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
			bufferCreateInfo.queueFamilyIndexCount = 3;
			bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

			VKAllocationCreateInfo allocCreateInfo = {};
			allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			allocCreateInfo.m_preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

			for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
			{
				m_uboBuffers[i].create(bufferCreateInfo, allocCreateInfo);
				m_mappableUBOBlock[i] = std::make_unique<VKMappableBufferBlock>(m_uboBuffers[i], g_context.m_properties.limits.minUniformBufferOffsetAlignment);
			}
		}

		// ssbo
		{
			VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bufferCreateInfo.size = RendererConsts::MAPPABLE_SSBO_BLOCK_SIZE;
			bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
			bufferCreateInfo.queueFamilyIndexCount = 3;
			bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

			VKAllocationCreateInfo allocCreateInfo = {};
			allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			allocCreateInfo.m_preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

			for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
			{
				m_ssboBuffers[i].create(bufferCreateInfo, allocCreateInfo);
				m_mappableSSBOBlock[i] = std::make_unique<VKMappableBufferBlock>(m_ssboBuffers[i], g_context.m_properties.limits.minStorageBufferOffsetAlignment);
			}
		}
	}

	// avg luminance buffer
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = sizeof(float) * RendererConsts::FRAMES_IN_FLIGHT;
		bufferCreateInfo.size = bufferCreateInfo.size < 32 ? 32 : bufferCreateInfo.size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_avgLuminanceBuffer.create(bufferCreateInfo, allocCreateInfo);
	}

	// luminance histogram readback buffers
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = sizeof(uint32_t) * RendererConsts::LUMINANCE_HISTOGRAM_SIZE;
		bufferCreateInfo.size = bufferCreateInfo.size < 32 ? 32 : bufferCreateInfo.size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			m_luminanceHistogramReadBackBuffers[i].create(bufferCreateInfo, allocCreateInfo);
		}
	}

	// luminance histogram readback buffers
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = sizeof(uint32_t);
		bufferCreateInfo.size = bufferCreateInfo.size < 32 ? 32 : bufferCreateInfo.size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			m_occlusionCullStatsReadBackBuffers[i].create(bufferCreateInfo, allocCreateInfo);
		}
	}

	// staging buffer
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = RendererConsts::STAGING_BUFFER_SIZE;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		m_stagingBuffer.create(bufferCreateInfo, allocCreateInfo);
	}

	// material buffer
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = sizeof(MaterialData) * RendererConsts::MAX_MATERIALS;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_materialBuffer.create(bufferCreateInfo, allocCreateInfo);
	}

	// vertex buffer
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal) + sizeof(VertexTexCoord));
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_vertexBuffer.create(bufferCreateInfo, allocCreateInfo);
	}

	// index buffer
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = RendererConsts::MAX_INDICES * sizeof(uint16_t);
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_indexBuffer.create(bufferCreateInfo, allocCreateInfo);
	}

	// submeshdata info buffer
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = RendererConsts::MAX_SUB_MESHES * sizeof(SubMeshInfo);
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_subMeshDataInfoBuffer.create(bufferCreateInfo, allocCreateInfo);
	}

	// submesh bounding box buffer
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = RendererConsts::MAX_SUB_MESHES * sizeof(float) * 6;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_subMeshBoundingBoxBuffer.create(bufferCreateInfo, allocCreateInfo);
	}

	// shadow sampler
	{
		VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.compareEnable = VK_TRUE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_GREATER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_shadowSampler) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}
	}

	// linear sampler
	{
		VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.maxAnisotropy = g_context.m_properties.limits.maxSamplerAnisotropy;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 14.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

		// clamp
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}

		// repeat
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}
	}

	// point sampler
	{
		VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.maxAnisotropy = g_context.m_properties.limits.maxSamplerAnisotropy;
		samplerCreateInfo.compareEnable = VK_FALSE;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 14.0f;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

		// clamp
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}

		// repeat
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_samplers[RendererConsts::SAMPLER_POINT_REPEAT_IDX]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}
	}

	// create texture descriptor pool
	{
		VkDescriptorPoolSize poolSizes[]
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE , RendererConsts::TEXTURE_ARRAY_SIZE * 2 + 1 /*ImGui Fonts texture*/ },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 4 * 3 }
		};

		VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolCreateInfo.maxSets = 3;
		poolCreateInfo.poolSizeCount = 2;
		poolCreateInfo.pPoolSizes = poolSizes;

		if (vkCreateDescriptorPool(g_context.m_device, &poolCreateInfo, nullptr, &m_textureDescriptorSetPool) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create texture array DescriptorPool!", -1);
		}
	}

	// create texture descriptor set
	{
		VkDescriptorSetLayoutBinding bindings[]
		{
			{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, RendererConsts::TEXTURE_ARRAY_SIZE, VK_SHADER_STAGE_FRAGMENT_BIT},
			{1, VK_DESCRIPTOR_TYPE_SAMPLER, 4, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		};

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutCreateInfo.bindingCount = 2;
		layoutCreateInfo.pBindings = bindings;

		if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutCreateInfo, nullptr, &m_textureDescriptorSetLayout) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create texture array DescriptorSetLayout!", -1);
		}

		VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		setAllocInfo.descriptorPool = m_textureDescriptorSetPool;
		setAllocInfo.descriptorSetCount = 1;
		setAllocInfo.pSetLayouts = &m_textureDescriptorSetLayout;

		if (vkAllocateDescriptorSets(g_context.m_device, &setAllocInfo, &m_textureDescriptorSet) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create texture array DescriptorSet!", -1);
		}
	}

	// create compute texture descriptor set
	{
		VkDescriptorSetLayoutBinding bindings[]
		{
			{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, RendererConsts::TEXTURE_ARRAY_SIZE, VK_SHADER_STAGE_COMPUTE_BIT},
			{1, VK_DESCRIPTOR_TYPE_SAMPLER, 4, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
		};

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutCreateInfo.bindingCount = 2;
		layoutCreateInfo.pBindings = bindings;

		if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutCreateInfo, nullptr, &m_computeTextureDescriptorSetLayout) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create texture array DescriptorSetLayout!", -1);
		}

		VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		setAllocInfo.descriptorPool = m_textureDescriptorSetPool;
		setAllocInfo.descriptorSetCount = 1;
		setAllocInfo.pSetLayouts = &m_computeTextureDescriptorSetLayout;

		if (vkAllocateDescriptorSets(g_context.m_device, &setAllocInfo, &m_computeTextureDescriptorSet) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create texture array DescriptorSet!", -1);
		}
	}

	// create ImGui descriptor set
	{
		VkDescriptorSetLayoutBinding bindings[]
		{
			{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
			{1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX]},
		};

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutCreateInfo.bindingCount = 2;
		layoutCreateInfo.pBindings = bindings;

		if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutCreateInfo, nullptr, &m_imGuiDescriptorSetLayout) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create ImGui DescriptorSetLayout!", -1);
		}

		VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		setAllocInfo.descriptorPool = m_textureDescriptorSetPool;
		setAllocInfo.descriptorSetCount = 1;
		setAllocInfo.pSetLayouts = &m_imGuiDescriptorSetLayout;

		if (vkAllocateDescriptorSets(g_context.m_device, &setAllocInfo, &m_imGuiDescriptorSet) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create ImGui DescriptorSet!", -1);
		}
	}

	createImGuiFontsTexture();

	// light proxy vertex/index buffer
	{
		float vertexData[] =
		{
			 0.0f, -1.0f, 0.0f,
			 0.723607f, -0.447220f, 0.525725f,
			 -0.276388f, -0.447220f, 0.850649f,
			 -0.894426f, -0.447216f, 0.0f,
			 -0.276388f, -0.447220f, -0.850649f,
			 0.723607f, -0.447220f, -0.525725f,
			 0.276388f, 0.447220f, 0.850649f,
			 -0.723607f, 0.447220f, 0.525725f,
			 -0.723607f, 0.447220f, -0.525725f,
			 0.276388f, 0.447220f, -0.850649f,
			 0.894426f, 0.447216f, 0.0f,
			 0.0f, 1.0f, 0.0f,
			 -0.162456f, -0.850654f, 0.499995f,
			 0.425323f, -0.850654f, 0.309011f,
			 0.262869f, -0.525738f, 0.809012f,
			 0.850648f, -0.525736f, 0.0f,
			 0.425323f, -0.850654f, -0.309011f,
			 -0.525730f, -0.850652f, 0.0f,
			 -0.688189f, -0.525736f, 0.499997f,
			 -0.162456f, -0.850654f, -0.499995f,
			 -0.688189f, -0.525736f, -0.499997f,
			 0.262869f, -0.525738f, -0.809012f,
			 0.951058f, 0.0f, 0.309013f,
			 0.951058f, 0.0f, -0.309013f,
			 0.0f, 0.0f, 1.0f,
			 0.587786f, 0.0f, 0.809017f,
			 -0.951058f, 0.0f, 0.309013f,
			 -0.587786f, 0.0f, 0.809017f,
			 -0.587786f, 0.0f, -0.809017f,
			 -0.951058f, 0.0f, -0.309013f,
			 0.587786f, 0.0f, -0.809017f,
			 0.0f, 0.0f, -1.0f,
			 0.688189f, 0.525736f, 0.499997f,
			 -0.262869f, 0.525738f, 0.809012f,
			 -0.850648f, 0.525736f, 0.0f,
			 -0.262869f, 0.525738f, -0.809012f,
			 0.688189f, 0.525736f, -0.499997f,
			 0.162456f, 0.850654f, 0.499995f,
			 0.525730f, 0.850652f, 0.0f,
			 -0.425323f, 0.850654f, 0.309011f,
			 -0.425323f, 0.850654f, -0.309011f,
			 0.162456f, 0.850654f, -0.499995f,
		};

		uint16_t indexData[] =
		{
			0, 13, 12,
			1, 13, 15,
			0, 12, 17,
			0, 17, 19,
			0, 19, 16,
			1, 15, 22,
			2, 14, 24,
			3, 18, 26,
			4, 20, 28,
			5, 21, 30,
			1, 22, 25,
			2, 24, 27,
			3, 26, 29,
			4, 28, 31,
			5, 30, 23,
			6, 32, 37,
			7, 33, 39,
			8, 34, 40,
			9, 35, 41,
			10, 36, 38,
			38, 41, 11,
			38, 36, 41,
			36, 9, 41,
			41, 40, 11,
			41, 35, 40,
			35, 8, 40,
			40, 39, 11,
			40, 34, 39,
			34, 7, 39,
			39, 37, 11,
			39, 33, 37,
			33, 6, 37,
			37, 38, 11,
			37, 32, 38,
			32, 10, 38,
			23, 36, 10,
			23, 30, 36,
			30, 9, 36,
			31, 35, 9,
			31, 28, 35,
			28, 8, 35,
			29, 34, 8,
			29, 26, 34,
			26, 7, 34,
			27, 33, 7,
			27, 24, 33,
			24, 6, 33,
			25, 32, 6,
			25, 22, 32,
			22, 10, 32,
			30, 31, 9,
			30, 21, 31,
			21, 4, 31,
			28, 29, 8,
			28, 20, 29,
			20, 3, 29,
			26, 27, 7,
			26, 18, 27,
			18, 2, 27,
			24, 25, 6,
			24, 14, 25,
			14, 1, 25,
			22, 23, 10,
			22, 15, 23,
			15, 5, 23,
			16, 21, 5,
			16, 19, 21,
			19, 4, 21,
			19, 20, 4,
			19, 17, 20,
			17, 3, 20,
			17, 18, 3,
			17, 12, 18,
			12, 2, 18,
			15, 16, 5,
			15, 13, 16,
			13, 0, 16,
			12, 14, 2,
			12, 13, 14,
			13, 1, 14,
		};

		VkBufferCreateInfo vertexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		vertexBufferInfo.size = sizeof(vertexData);
		vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_lightProxyVertexBuffer.create(vertexBufferInfo, allocCreateInfo);

		VkBufferCreateInfo indexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		indexBufferInfo.size = sizeof(indexData);
		indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_lightProxyIndexBuffer.create(indexBufferInfo, allocCreateInfo);

		void *data;
		g_context.m_allocator.mapMemory(m_stagingBuffer.getAllocation(), &data);
		memcpy(data, vertexData, sizeof(vertexData));
		memcpy(((unsigned char *)data) + sizeof(vertexData), indexData, sizeof(indexData));
		g_context.m_allocator.unmapMemory(m_stagingBuffer.getAllocation());

		VkCommandBuffer commandBuffer = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		{
			VkBufferCopy copyRegionVertex = { 0, 0, sizeof(vertexData) };
			vkCmdCopyBuffer(commandBuffer, m_stagingBuffer.getBuffer(), m_lightProxyVertexBuffer.getBuffer(), 1, &copyRegionVertex);
			VkBufferCopy copyRegionIndex = { sizeof(vertexData), 0, sizeof(indexData) };
			vkCmdCopyBuffer(commandBuffer, m_stagingBuffer.getBuffer(), m_lightProxyIndexBuffer.getBuffer(), 1, &copyRegionIndex);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, commandBuffer);
	}

	// box index buffer
	{
		uint16_t indices[] =
		{
			0, 1, 3,
			0, 3, 2,
			3, 7, 6,
			3, 6, 2,
			1, 5, 7,
			1, 7, 3,
			5, 4, 6,
			5, 6, 7,
			0, 4, 5,
			0, 5, 1,
			2, 6, 4,
			2, 4, 0
		};

		VkBufferCreateInfo indexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		indexBufferInfo.size = sizeof(indices);
		indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_boxIndexBuffer.create(indexBufferInfo, allocCreateInfo);

		void *data;
		g_context.m_allocator.mapMemory(m_stagingBuffer.getAllocation(), &data);
		memcpy(data, indices, sizeof(indices));
		g_context.m_allocator.unmapMemory(m_stagingBuffer.getAllocation());

		VkCommandBuffer commandBuffer = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		{
			VkBufferCopy copyRegionIndex = { 0, 0, sizeof(indices) };
			vkCmdCopyBuffer(commandBuffer, m_stagingBuffer.getBuffer(), m_boxIndexBuffer.getBuffer(), 1, &copyRegionIndex);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, commandBuffer);
	}
}

void VEngine::VKRenderResources::resize(uint32_t width, uint32_t height)
{
}

void VEngine::VKRenderResources::updateTextureArray(const VkDescriptorImageInfo *data, size_t count)
{
	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstSet = m_textureDescriptorSet;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorCount = count < RendererConsts::TEXTURE_ARRAY_SIZE ? static_cast<uint32_t>(count) : RendererConsts::TEXTURE_ARRAY_SIZE;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.pImageInfo = data;

	VkDescriptorImageInfo samplerInfo[4]{};
	for (size_t i = 0; i < 4; ++i)
	{
		samplerInfo[i].sampler = m_samplers[i];
	}

	VkWriteDescriptorSet samplerWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	samplerWrite.dstSet = m_textureDescriptorSet;
	samplerWrite.dstBinding = 1;
	samplerWrite.dstArrayElement = 0;
	samplerWrite.descriptorCount = 4;
	samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	samplerWrite.pImageInfo = samplerInfo;

	VkWriteDescriptorSet writes[]{ write, samplerWrite };

	vkDeviceWaitIdle(g_context.m_device);
	vkUpdateDescriptorSets(g_context.m_device, 2, writes, 0, nullptr);

	writes[0].dstSet = m_computeTextureDescriptorSet;
	writes[1].dstSet = m_computeTextureDescriptorSet;
	vkUpdateDescriptorSets(g_context.m_device, 2, writes, 0, nullptr);
}

void VEngine::VKRenderResources::createImGuiFontsTexture()
{
	ImGuiIO& io = ImGui::GetIO();

	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	size_t upload_size = width * height * 4 * sizeof(char);

	assert(upload_size <= m_stagingBuffer.getSize());

	// create image and view
	{
		// create image
		VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCreateInfo.extent.width = width;
		imageCreateInfo.extent.height = height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_imGuiFontsTexture.create(imageCreateInfo, allocCreateInfo);


		// create view
		VkImageViewCreateInfo viewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewCreateInfo.image = m_imGuiFontsTexture.getImage();
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = imageCreateInfo.format;
		viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		if (vkCreateImageView(g_context.m_device, &viewCreateInfo, nullptr, &m_imGuiFontsTextureView) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create ImGui fonts texture view!", EXIT_FAILURE);
		}
	}

	// Update the Descriptor Set:
	{
		VkDescriptorImageInfo imageInfo{ VK_NULL_HANDLE, m_imGuiFontsTextureView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

		VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = m_imGuiDescriptorSet;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(g_context.m_device, 1, &write, 0, nullptr);
	}

	// Upload to Buffer:
	{
		uint8_t *map = nullptr;
		g_context.m_allocator.mapMemory(m_stagingBuffer.getAllocation(), (void **)&map);
		{
			memcpy(map, pixels, upload_size);
		}
		g_context.m_allocator.unmapMemory(m_stagingBuffer.getAllocation());
	}

	// Copy to Image:
	{
		VkCommandBuffer copyCmd = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		{
			VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.image = m_imGuiFontsTexture.getImage();
			imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

			// transition image into VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			imageBarrier.srcAccessMask = 0;
			imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

			VkBufferImageCopy region{};
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = width;
			region.imageExtent.height = height;
			region.imageExtent.depth = 1;

			vkCmdCopyBufferToImage(copyCmd, m_stagingBuffer.getBuffer(), m_imGuiFontsTexture.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			// transition image into VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, copyCmd);
	}

	// Store our identifier
	io.Fonts->TexID = (ImTextureID)(intptr_t)m_imGuiFontsTexture.getImage();
}

VEngine::VKRenderResources::VKRenderResources()
{
}
