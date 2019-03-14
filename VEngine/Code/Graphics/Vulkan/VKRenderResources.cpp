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
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_linearSampler) != VK_SUCCESS)
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
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 1;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		if (vkCreateSampler(g_context.m_device, &samplerCreateInfo, nullptr, &m_pointSampler) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create sampler!", -1);
		}
	}

	// create descriptor set layouts
	{
		// per frame
		{
			VkDescriptorSetLayoutBinding perFrameLayoutBinding = {};
			perFrameLayoutBinding.binding = 0;
			perFrameLayoutBinding.descriptorCount = 1;
			perFrameLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			perFrameLayoutBinding.pImmutableSamplers = nullptr;
			perFrameLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings = &perFrameLayoutBinding;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_perFrameDataDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// per draw
		{
			VkDescriptorSetLayoutBinding perDrawLayoutBinding = {};
			perDrawLayoutBinding.binding = 0;
			perDrawLayoutBinding.descriptorCount = 1;
			perDrawLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			perDrawLayoutBinding.pImmutableSamplers = nullptr;
			perDrawLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings = &perDrawLayoutBinding;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_perDrawDataDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// texture array descriptor set layout
		{
			VkDescriptorSetLayoutBinding textureArrayLayoutBinding = {};
			textureArrayLayoutBinding.binding = 0;
			textureArrayLayoutBinding.descriptorCount = TEXTURE_ARRAY_SIZE;
			textureArrayLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			textureArrayLayoutBinding.pImmutableSamplers = nullptr;
			textureArrayLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings = &textureArrayLayoutBinding;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_textureDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// lighting input
		{
			VkDescriptorSetLayoutBinding lightingInputLayoutBindings[2] = {};

			// result image
			lightingInputLayoutBindings[0].binding = 0;
			lightingInputLayoutBindings[0].descriptorCount = 1;
			lightingInputLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			lightingInputLayoutBindings[0].pImmutableSamplers = nullptr;
			lightingInputLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// gbuffer textures
			lightingInputLayoutBindings[1].binding = 1;
			lightingInputLayoutBindings[1].descriptorCount = 4;
			lightingInputLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			lightingInputLayoutBindings[1].pImmutableSamplers = nullptr;
			lightingInputLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = sizeof(lightingInputLayoutBindings) / sizeof(lightingInputLayoutBindings[0]);
			layoutInfo.pBindings = lightingInputLayoutBindings;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_lightingInputDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// light data
		{
			VkDescriptorSetLayoutBinding lightDataLayoutBindings[6] = {};

			// directional light data
			lightDataLayoutBindings[0].binding = 0;
			lightDataLayoutBindings[0].descriptorCount = 1;
			lightDataLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightDataLayoutBindings[0].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// point light data
			lightDataLayoutBindings[1].binding = 1;
			lightDataLayoutBindings[1].descriptorCount = 1;
			lightDataLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightDataLayoutBindings[1].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// spot light data
			lightDataLayoutBindings[2].binding = 2;
			lightDataLayoutBindings[2].descriptorCount = 1;
			lightDataLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightDataLayoutBindings[2].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// shadow data
			lightDataLayoutBindings[3].binding = 3;
			lightDataLayoutBindings[3].descriptorCount = 1;
			lightDataLayoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightDataLayoutBindings[3].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// point light zbins
			lightDataLayoutBindings[4].binding = 4;
			lightDataLayoutBindings[4].descriptorCount = 1;
			lightDataLayoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightDataLayoutBindings[4].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			// shadow texture
			lightDataLayoutBindings[5].binding = 5;
			lightDataLayoutBindings[5].descriptorCount = 1;
			lightDataLayoutBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			lightDataLayoutBindings[5].pImmutableSamplers = nullptr;
			lightDataLayoutBindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = sizeof(lightDataLayoutBindings) / sizeof(lightDataLayoutBindings[0]);
			layoutInfo.pBindings = lightDataLayoutBindings;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_lightDataDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// cull data
		{
			VkDescriptorSetLayoutBinding cullDataLayoutBindings[1] = {};

			// point light data
			cullDataLayoutBindings[0].binding = 0;
			cullDataLayoutBindings[0].descriptorCount = 1;
			cullDataLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			cullDataLayoutBindings[0].pImmutableSamplers = nullptr;
			cullDataLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = sizeof(cullDataLayoutBindings) / sizeof(cullDataLayoutBindings[0]);
			layoutInfo.pBindings = cullDataLayoutBindings;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_cullDataDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}

		// light bit mask data
		{
			VkDescriptorSetLayoutBinding lightIndexDataLayoutBindings[1] = {};

			// point light bit mask
			lightIndexDataLayoutBindings[0].binding = 0;
			lightIndexDataLayoutBindings[0].descriptorCount = 1;
			lightIndexDataLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			lightIndexDataLayoutBindings[0].pImmutableSamplers = nullptr;
			lightIndexDataLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutInfo.bindingCount = sizeof(lightIndexDataLayoutBindings) / sizeof(lightIndexDataLayoutBindings[0]);
			layoutInfo.pBindings = lightIndexDataLayoutBindings;

			if (vkCreateDescriptorSetLayout(g_context.m_device, &layoutInfo, nullptr, &m_lightBitMaskDescriptorSetLayout) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create descriptor set layout!", -1);
			}
		}
	}

	// create descriptor set pool
	{
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , 1 * FRAMES_IN_FLIGHT },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC , 1 * FRAMES_IN_FLIGHT },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER , TEXTURE_ARRAY_SIZE + 5 * FRAMES_IN_FLIGHT },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 * FRAMES_IN_FLIGHT },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * FRAMES_IN_FLIGHT }
		};

		VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolInfo.poolSizeCount = static_cast<uint32_t>(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = 7 * FRAMES_IN_FLIGHT;

		if (vkCreateDescriptorPool(g_context.m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create descriptor pool!", -1);
		}
	}

	// create descriptor sets
	{
		VkDescriptorSetLayout layouts[] =
		{
			m_perFrameDataDescriptorSetLayout,
			m_perDrawDataDescriptorSetLayout,
			m_lightingInputDescriptorSetLayout,
			m_lightDataDescriptorSetLayout,
			m_cullDataDescriptorSetLayout,
			m_lightBitMaskDescriptorSetLayout
		};

		VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_textureDescriptorSetLayout;

		// allocate textures descriptor set
		if (vkAllocateDescriptorSets(g_context.m_device, &allocInfo, &m_textureDescriptorSet) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to allocate descriptor set!", -1);
		}


		allocInfo.descriptorSetCount = static_cast<uint32_t>(sizeof(layouts) / sizeof(layouts[0]));
		allocInfo.pSetLayouts = layouts;

		for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			VkDescriptorSet sets[] =
			{
				m_perFrameDataDescriptorSets[i],
				m_perDrawDataDescriptorSets[i],
				m_lightingInputDescriptorSets[i],
				m_lightDataDescriptorSets[i],
				m_cullDataDescriptorSets[i],
				m_lightBitMaskDescriptorSets[i],
			};

			if (vkAllocateDescriptorSets(g_context.m_device, &allocInfo, sets) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to allocate descriptor set!", -1);
			}

			m_perFrameDataDescriptorSets[i] = sets[0];
			m_perDrawDataDescriptorSets[i] = sets[1];
			m_lightingInputDescriptorSets[i] = sets[2];
			m_lightDataDescriptorSets[i] = sets[3];
			m_cullDataDescriptorSets[i] = sets[4];
			m_lightBitMaskDescriptorSets[i] = sets[5];
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
	VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	descriptorWrite.dstSet = m_textureDescriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = count < TEXTURE_ARRAY_SIZE ? static_cast<uint32_t>(count) : TEXTURE_ARRAY_SIZE;
	descriptorWrite.pImageInfo = data;

	vkQueueWaitIdle(g_context.m_graphicsQueue);
	vkUpdateDescriptorSets(g_context.m_device, 1, &descriptorWrite, 0, nullptr);
}

VEngine::VKRenderResources::VKRenderResources()
	:m_vertexBuffer(),
	m_indexBuffer()
{
}
