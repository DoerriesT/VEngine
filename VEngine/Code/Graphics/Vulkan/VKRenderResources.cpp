#include "VKRenderResources.h"
#include "VKUtility.h"
#include "VKContext.h"
#include "Utility/Utility.h"
#include "Graphics/RenderParams.h"
#include "VKTextureLoader.h"
#include "GlobalVar.h"
#include "Graphics/LightData.h"
#include "Graphics/DrawItem.h"
#include "VKSyncPrimitiveAllocator.h"
#include "VKPipelineManager.h"

VEngine::VKRenderResources::~VKRenderResources()
{

}

void VEngine::VKRenderResources::init(unsigned int width, unsigned int height, VkFormat swapchainFormat)
{
	m_syncPrimitiveAllocator = std::make_unique<VKSyncPrimitiveAllocator>();
	m_pipelineManager = std::make_unique<VKPipelineManager>();

	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		m_shadowTextureSemaphores[i] = m_syncPrimitiveAllocator->acquireSemaphore();
		m_swapChainImageAvailableSemaphores[i] = m_syncPrimitiveAllocator->acquireSemaphore();
		m_swapChainRenderFinishedSemaphores[i] = m_syncPrimitiveAllocator->acquireSemaphore();
	}

	// shadow atlas
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_D16_UNORM;
		imageCreateInfo.extent.width = g_shadowAtlasSize;
		imageCreateInfo.extent.height = g_shadowAtlasSize;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_shadowTexture.create(imageCreateInfo, allocCreateInfo);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.layerCount = 1;

		VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = m_shadowTexture.getImage();
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_shadowTexture.getFormat();
		viewInfo.subresourceRange = subresourceRange;

		if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_shadowTextureView) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image view!", -1);
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

		for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			m_taaHistoryTextures[i].create(imageCreateInfo, allocCreateInfo);

			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = 1;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.layerCount = 1;

			VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			viewInfo.image = m_taaHistoryTextures[i].getImage();
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = m_taaHistoryTextures[i].getFormat();
			viewInfo.subresourceRange = subresourceRange;

			if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_taaHistoryTextureViews[i]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create image view!", -1);
			}
		}
	}

	// avg luminance buffer
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = sizeof(float) * FRAMES_IN_FLIGHT;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_avgLuminanceBuffer.create(bufferCreateInfo, allocCreateInfo);

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
		samplerCreateInfo.compareOp = VK_COMPARE_OP_GREATER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

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
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		// clamp
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	
		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_linearSamplerClamp) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}

		// repeat
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_linearSamplerRepeat) != VK_SUCCESS)
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
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		// clamp
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_pointSamplerClamp) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}

		// repeat
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_pointSamplerRepeat) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}
	}

	// create descriptor sets
	{
		// create layout
		VkDescriptorSetLayoutBinding bindings[32] = {};
		uint32_t bindingCount = 0;

		// common set
		{
			uint32_t offset = bindingCount;

			// general data
			bindings[bindingCount].binding = CommonSetBindings::PER_FRAME_DATA_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// per draw data
			bindings[bindingCount].binding = CommonSetBindings::PER_DRAW_DATA_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			++bindingCount;

			// shadow data
			bindings[bindingCount].binding = CommonSetBindings::SHADOW_DATA_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// directional light data
			bindings[bindingCount].binding = CommonSetBindings::DIRECTIONAL_LIGHT_DATA_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// point light data
			bindings[bindingCount].binding = CommonSetBindings::POINT_LIGHT_DATA_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// point light zbins
			bindings[bindingCount].binding = CommonSetBindings::POINT_LIGHT_Z_BINS_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// point light bit mask
			bindings[bindingCount].binding = CommonSetBindings::POINT_LIGHT_BITMASK_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			++bindingCount;

			// persistent values
			bindings[bindingCount].binding = CommonSetBindings::PERSISTENT_VALUES_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// luminance histogram
			bindings[bindingCount].binding = CommonSetBindings::LUMINANCE_HISTOGRAM_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			++bindingCount;

			// textures
			bindings[bindingCount].binding = CommonSetBindings::TEXTURES_BINDING;
			bindings[bindingCount].descriptorCount = TEXTURE_ARRAY_SIZE;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			++bindingCount;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = bindingCount - offset;
			layoutInfo.pBindings = &bindings[offset];

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_descriptorSetLayouts[COMMON_SET_INDEX]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}
		
		// lighting set
		{
			uint32_t offset = bindingCount;

			// lighting result image
			bindings[bindingCount].binding = LightingSetBindings::RESULT_IMAGE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// gbuffer textures
			bindings[bindingCount].binding = LightingSetBindings::G_BUFFER_TEXTURES_BINDING;
			bindings[bindingCount].descriptorCount = 4;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// shadow texture
			bindings[bindingCount].binding = LightingSetBindings::SHADOW_TEXTURE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = bindingCount - offset;
			layoutInfo.pBindings = &bindings[offset];

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_descriptorSetLayouts[LIGHTING_SET_INDEX]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}


		// luminance histogram set
		{
			uint32_t offset = bindingCount;

			// source texture
			bindings[bindingCount].binding = LuminanceHistogramSetBindings::SOURCE_TEXTURE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = bindingCount - offset;
			layoutInfo.pBindings = &bindings[offset];

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_descriptorSetLayouts[LUMINANCE_HISTOGRAM_SET_INDEX]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}
		

		// tonemap set
		{
			uint32_t offset = bindingCount;

			// tonemap result image
			bindings[bindingCount].binding = TonemapSetBindings::RESULT_IMAGE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// source texture
			bindings[bindingCount].binding = TonemapSetBindings::SOURCE_TEXTURE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = bindingCount - offset;
			layoutInfo.pBindings = &bindings[offset];

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_descriptorSetLayouts[TONEMAP_SET_INDEX]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// taa resolve set
		{
			uint32_t offset = bindingCount;

			// result image
			bindings[bindingCount].binding = TAAResolveSetBindings::RESULT_IMAGE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// depth texture
			bindings[bindingCount].binding = TAAResolveSetBindings::DEPTH_TEXTURE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// depth texture
			bindings[bindingCount].binding = TAAResolveSetBindings::VELOCITY_TEXTURE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// history image
			bindings[bindingCount].binding = TAAResolveSetBindings::HISTORY_IMAGE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// source texture
			bindings[bindingCount].binding = TAAResolveSetBindings::SOURCE_TEXTURE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = bindingCount - offset;
			layoutInfo.pBindings = &bindings[offset];

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_descriptorSetLayouts[TAA_RESOLVE_SET_INDEX]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// velocity composition set
		{
			uint32_t offset = bindingCount;

			// result image
			bindings[bindingCount].binding = VelocityCompositionSetBindings::VELOCITY_IMAGE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			// depth texture
			bindings[bindingCount].binding = VelocityCompositionSetBindings::DEPTH_IMAGE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			++bindingCount;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = bindingCount - offset;
			layoutInfo.pBindings = &bindings[offset];

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_descriptorSetLayouts[VELOCITY_COMPOSITION_SET_INDEX]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// velocity initialization set
		{
			uint32_t offset = bindingCount;

			// depth texture
			bindings[bindingCount].binding = VelocityInitializationSetBindings::DEPTH_IMAGE_BINDING;
			bindings[bindingCount].descriptorCount = 1;
			bindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			bindings[bindingCount].pImmutableSamplers = nullptr;
			bindings[bindingCount].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			++bindingCount;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = bindingCount - offset;
			layoutInfo.pBindings = &bindings[offset];

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_descriptorSetLayouts[VELOCITY_INITIALIZATION_SET_INDEX]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}


		// create descriptor pool

		uint32_t poolSizesMap[VK_DESCRIPTOR_TYPE_RANGE_SIZE] = {};
		for (uint32_t i = 0; i < bindingCount; ++i)
		{
			poolSizesMap[bindings[i].descriptorType] += bindings[i].descriptorCount * FRAMES_IN_FLIGHT;
		}

		VkDescriptorPoolSize poolSizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
		uint32_t poolSizeCount = 0;
		for (uint32_t i = 0; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i)
		{
			if (poolSizesMap[i])
			{
				poolSizes[poolSizeCount++] = { VkDescriptorType(i), poolSizesMap[i] };
			}
		}

		VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolInfo.poolSizeCount = poolSizeCount;
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = FRAMES_IN_FLIGHT * (MAX_DESCRIPTOR_SET_INDEX + 1);

		if (vkCreateDescriptorPool(g_context.m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create descriptor pool!", -1);
		}



		// create sets

		VkDescriptorSetLayout layouts[FRAMES_IN_FLIGHT * (MAX_DESCRIPTOR_SET_INDEX + 1)];
		for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			for (size_t j = 0; j < MAX_DESCRIPTOR_SET_INDEX + 1; ++j)
			{
				layouts[i * (MAX_DESCRIPTOR_SET_INDEX + 1) + j] = m_descriptorSetLayouts[j];
			}
		}

		VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = FRAMES_IN_FLIGHT * (MAX_DESCRIPTOR_SET_INDEX + 1);
		allocInfo.pSetLayouts = layouts;

		if (vkAllocateDescriptorSets(g_context.m_device, &allocInfo, reinterpret_cast<VkDescriptorSet *>(m_descriptorSets)) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to allocate descriptor sets!", -1);
		}
	}

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



		VKBuffer stagingBuffer;

		VkBufferCreateInfo stagingBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		stagingBufferInfo.size = sizeof(vertexData) + sizeof(indexData);
		stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		stagingBuffer.create(stagingBufferInfo, allocCreateInfo);

		void *data;
		g_context.m_allocator.mapMemory(stagingBuffer.getAllocation(), &data);
		memcpy(data, vertexData, sizeof(vertexData));
		memcpy(((unsigned char *)data) + sizeof(vertexData), indexData, sizeof(indexData));
		g_context.m_allocator.unmapMemory(stagingBuffer.getAllocation());

		VkCommandBuffer commandBuffer = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		{
			VkBufferCopy copyRegionVertex = { 0, 0, sizeof(vertexData) };
			vkCmdCopyBuffer(commandBuffer, stagingBuffer.getBuffer(), m_lightProxyVertexBuffer.getBuffer(), 1, &copyRegionVertex);
			VkBufferCopy copyRegionIndex = { sizeof(vertexData), 0, sizeof(indexData) };
			vkCmdCopyBuffer(commandBuffer, stagingBuffer.getBuffer(), m_lightProxyIndexBuffer.getBuffer(), 1, &copyRegionIndex);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, commandBuffer);

		stagingBuffer.destroy();
	}
}

void VEngine::VKRenderResources::resize(unsigned int width, unsigned int height)
{
}

void VEngine::VKRenderResources::reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize)
{
	if (!m_vertexBuffer.isValid() || m_vertexBuffer.getSize() < vertexSize)
	{
		m_vertexBuffer.destroy();

		VkBufferCreateInfo vertexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		vertexBufferInfo.size = vertexSize;
		vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		vertexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_vertexBuffer.create(vertexBufferInfo, allocCreateInfo);
	}

	if (!m_indexBuffer.isValid() || m_indexBuffer.getSize() < vertexSize)
	{
		m_indexBuffer.destroy();

		VkBufferCreateInfo indexBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		indexBufferInfo.size = indexSize;
		indexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		indexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		m_indexBuffer.create(indexBufferInfo, allocCreateInfo);
	}
}

void VEngine::VKRenderResources::uploadMeshData(const unsigned char *vertices, uint64_t vertexSize, const unsigned char *indices, uint64_t indexSize)
{
	VKBuffer stagingBuffer;

	VkBufferCreateInfo stagingBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	stagingBufferInfo.size = vertexSize + indexSize;
	stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VKAllocationCreateInfo allocCreateInfo = {};
	allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	stagingBuffer.create(stagingBufferInfo, allocCreateInfo);

	void *data;
	g_context.m_allocator.mapMemory(stagingBuffer.getAllocation(), &data);
	memcpy(data, vertices, (size_t)vertexSize);
	memcpy(((unsigned char *)data) + vertexSize, indices, (size_t)indexSize);
	g_context.m_allocator.unmapMemory(stagingBuffer.getAllocation());

	VkCommandBuffer commandBuffer = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
	{
		VkBufferCopy copyRegionVertex = { 0, 0, vertexSize };
		vkCmdCopyBuffer(commandBuffer, stagingBuffer.getBuffer(), m_vertexBuffer.getBuffer(), 1, &copyRegionVertex);
		VkBufferCopy copyRegionIndex = { vertexSize, 0, indexSize };
		vkCmdCopyBuffer(commandBuffer, stagingBuffer.getBuffer(), m_indexBuffer.getBuffer(), 1, &copyRegionIndex);
	}
	VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, commandBuffer);

	stagingBuffer.destroy();
}

void VEngine::VKRenderResources::updateTextureArray(const VkDescriptorImageInfo *data, size_t count)
{
	VkWriteDescriptorSet descriptorWrites[FRAMES_IN_FLIGHT];

	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		VkWriteDescriptorSet &write = descriptorWrites[i];
		write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = m_descriptorSets[i][COMMON_SET_INDEX];
		write.dstBinding = CommonSetBindings::TEXTURES_BINDING;
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = count < TEXTURE_ARRAY_SIZE ? static_cast<uint32_t>(count) : TEXTURE_ARRAY_SIZE;
		write.pImageInfo = data;
	}

	vkDeviceWaitIdle(g_context.m_device);
	vkUpdateDescriptorSets(g_context.m_device, FRAMES_IN_FLIGHT, descriptorWrites, 0, nullptr);
}

VEngine::VKRenderResources::VKRenderResources()
	:m_vertexBuffer(),
	m_indexBuffer()
{
}
