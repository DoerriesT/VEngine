#include "RenderResources.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "RenderData.h"
#include "Mesh.h"
#include "gal/Initializers.h"
#include "imgui/imgui.h"

using namespace VEngine::gal;

VEngine::RenderResources::RenderResources(gal::GraphicsDevice *graphicsDevice)
	:m_graphicsDevice(graphicsDevice)
{
	m_graphicsDevice->createCommandListPool(m_graphicsDevice->getGraphicsQueue(), &m_commandListPool);
	m_commandListPool->allocate(1, &m_commandList);
}

VEngine::RenderResources::~RenderResources()
{
	m_commandListPool->free(1, &m_commandList);
	m_graphicsDevice->destroyCommandListPool(m_commandListPool);

	// destroy all resources
	// per frame images/buffers
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_mappableUBOBlock[i].reset();
		m_mappableSSBOBlock[i].reset();

		m_graphicsDevice->destroyImage(m_depthImages[i]);
		m_graphicsDevice->destroyImage(m_lightImages[i]);
		m_graphicsDevice->destroyImage(m_taaHistoryTextures[i]);
		
		m_graphicsDevice->destroyBuffer(m_luminanceHistogramReadBackBuffers[i]);
		m_graphicsDevice->destroyBuffer(m_occlusionCullStatsReadBackBuffers[i]);
		m_graphicsDevice->destroyBuffer(m_uboBuffers[i]);
		m_graphicsDevice->destroyBuffer(m_ssboBuffers[i]);
	}
	
	// images
	m_graphicsDevice->destroyImage(m_imGuiFontsTexture);
	m_graphicsDevice->destroyImage(m_brdfLUT);

	// views
	m_graphicsDevice->destroyImageView(m_imGuiFontsTextureView);

	// buffers
	m_graphicsDevice->destroyBuffer(m_lightProxyVertexBuffer);
	m_graphicsDevice->destroyBuffer(m_lightProxyIndexBuffer);
	m_graphicsDevice->destroyBuffer(m_boxIndexBuffer);
	m_graphicsDevice->destroyBuffer(m_avgLuminanceBuffer);
	
	m_graphicsDevice->destroyBuffer(m_stagingBuffer);
	m_graphicsDevice->destroyBuffer(m_materialBuffer);
	m_graphicsDevice->destroyBuffer(m_vertexBuffer);
	m_graphicsDevice->destroyBuffer(m_indexBuffer);
	m_graphicsDevice->destroyBuffer(m_subMeshDataInfoBuffer);
	m_graphicsDevice->destroyBuffer(m_subMeshBoundingBoxBuffer);

	// samplers
	m_graphicsDevice->destroySampler(m_samplers[0]);
	m_graphicsDevice->destroySampler(m_samplers[1]);
	m_graphicsDevice->destroySampler(m_samplers[2]);
	m_graphicsDevice->destroySampler(m_samplers[3]);
	m_graphicsDevice->destroySampler(m_shadowSampler);
	
	m_graphicsDevice->destroyDescriptorSetLayout(m_textureDescriptorSetLayout);
	m_graphicsDevice->destroyDescriptorSetLayout(m_computeTextureDescriptorSetLayout);
	m_graphicsDevice->destroyDescriptorSetLayout(m_imGuiDescriptorSetLayout);

	m_textureDescriptorSetPool->freeDescriptorSets(1, &m_textureDescriptorSet);
	m_textureDescriptorSetPool->freeDescriptorSets(1, &m_computeTextureDescriptorSet);
	m_textureDescriptorSetPool->freeDescriptorSets(1, &m_imGuiDescriptorSet);

	m_graphicsDevice->destroyDescriptorPool(m_textureDescriptorSetPool);
}

void VEngine::RenderResources::init(uint32_t width, uint32_t height)
{
	// brdf LUT
	{
		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = 128;
		imageCreateInfo.m_height = 128;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = Format::R16G16_SFLOAT;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::SAMPLED_BIT | ImageUsageFlagBits::STORAGE_BIT;

		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_brdfLUT);
	}

	resize(width, height);

	// mappable blocks
	{
		// ubo
		{
			BufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.m_size = RendererConsts::MAPPABLE_UBO_BLOCK_SIZE;
			bufferCreateInfo.m_createFlags = 0;
			bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::UNIFORM_BUFFER_BIT;

			for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
			{
				m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::HOST_VISIBLE_BIT | MemoryPropertyFlagBits::HOST_COHERENT_BIT, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, false, &m_uboBuffers[i]);
				m_mappableUBOBlock[i] = std::make_unique<MappableBufferBlock>(m_uboBuffers[i], m_graphicsDevice->getMinStorageBufferOffsetAlignment());
			}
		}

		// ssbo
		{
			BufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.m_size = RendererConsts::MAPPABLE_SSBO_BLOCK_SIZE;
			bufferCreateInfo.m_createFlags = 0;
			bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::STORAGE_BUFFER_BIT;

			for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
			{
				m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::HOST_VISIBLE_BIT | MemoryPropertyFlagBits::HOST_COHERENT_BIT, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, false, &m_ssboBuffers[i]);
				m_mappableSSBOBlock[i] = std::make_unique<MappableBufferBlock>(m_ssboBuffers[i], m_graphicsDevice->getMinStorageBufferOffsetAlignment());
			}
		}
	}


	// avg luminance buffer
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = sizeof(float) * RendererConsts::FRAMES_IN_FLIGHT;
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::STORAGE_BUFFER_BIT;

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_avgLuminanceBuffer);
	}

	// luminance histogram readback buffers
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = sizeof(uint32_t) * RendererConsts::LUMINANCE_HISTOGRAM_SIZE;
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::HOST_VISIBLE_BIT | MemoryPropertyFlagBits::HOST_CACHED_BIT, 0, false, &m_luminanceHistogramReadBackBuffers[i]);
		}
	}

	// luminance histogram readback buffers
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = sizeof(uint32_t);
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::STORAGE_BUFFER_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::HOST_VISIBLE_BIT | MemoryPropertyFlagBits::HOST_CACHED_BIT, 0, false, &m_occlusionCullStatsReadBackBuffers[i]);
		}
	}

	// staging buffer
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = RendererConsts::STAGING_BUFFER_SIZE;
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_SRC_BIT;

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::HOST_VISIBLE_BIT | MemoryPropertyFlagBits::HOST_COHERENT_BIT, 0, false, &m_stagingBuffer);
	}

	// material buffer
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = sizeof(MaterialData) * RendererConsts::MAX_MATERIALS;
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::STORAGE_BUFFER_BIT;

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_materialBuffer);
	}

	// vertex buffer
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal) + sizeof(VertexTexCoord));
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::VERTEX_BUFFER_BIT | BufferUsageFlagBits::STORAGE_BUFFER_BIT;

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_vertexBuffer);
	}

	// index buffer
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = RendererConsts::MAX_INDICES * sizeof(uint16_t);
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::INDEX_BUFFER_BIT | BufferUsageFlagBits::STORAGE_BUFFER_BIT;

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_indexBuffer);
	}

	// submeshdata info buffer
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = RendererConsts::MAX_SUB_MESHES * sizeof(SubMeshInfo);
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::STORAGE_BUFFER_BIT;

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_subMeshDataInfoBuffer);
	}

	// submesh bounding box buffer
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = RendererConsts::MAX_SUB_MESHES * sizeof(float) * 6;
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::STORAGE_BUFFER_BIT;

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_subMeshBoundingBoxBuffer);
	}

	// shadow sampler
	{
		SamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.m_magFilter = Filter::LINEAR;
		samplerCreateInfo.m_minFilter = Filter::LINEAR;
		samplerCreateInfo.m_mipmapMode = SamplerMipmapMode::LINEAR;
		samplerCreateInfo.m_addressModeU = SamplerAddressMode::REPEAT;
		samplerCreateInfo.m_addressModeV = SamplerAddressMode::REPEAT;
		samplerCreateInfo.m_addressModeW = SamplerAddressMode::REPEAT;
		samplerCreateInfo.m_mipLodBias = 0.0f;
		samplerCreateInfo.m_anisotropyEnable = true;
		samplerCreateInfo.m_maxAnisotropy = 1.0f;
		samplerCreateInfo.m_compareEnable = true;
		samplerCreateInfo.m_compareOp = CompareOp::GREATER;
		samplerCreateInfo.m_minLod = 0.0f;
		samplerCreateInfo.m_maxLod = 1;
		samplerCreateInfo.m_borderColor = BorderColor::FLOAT_OPAQUE_WHITE;
		samplerCreateInfo.m_unnormalizedCoordinates = false;

		m_graphicsDevice->createSampler(samplerCreateInfo, &m_shadowSampler);
	}

	// linear sampler
	{
		SamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.m_magFilter = Filter::LINEAR;
		samplerCreateInfo.m_minFilter = Filter::LINEAR;
		samplerCreateInfo.m_mipmapMode = SamplerMipmapMode::LINEAR;
		samplerCreateInfo.m_mipLodBias = 0.0f;
		samplerCreateInfo.m_anisotropyEnable = true;
		samplerCreateInfo.m_maxAnisotropy = m_graphicsDevice->getMaxSamplerAnisotropy();
		samplerCreateInfo.m_compareEnable = false;
		samplerCreateInfo.m_compareOp = CompareOp::NEVER;
		samplerCreateInfo.m_minLod = 0.0f;
		samplerCreateInfo.m_maxLod = 14.0f;
		samplerCreateInfo.m_borderColor = BorderColor::FLOAT_OPAQUE_WHITE;
		samplerCreateInfo.m_unnormalizedCoordinates = false;

		// clamp
		samplerCreateInfo.m_addressModeU = SamplerAddressMode::CLAMP_TO_EDGE;
		samplerCreateInfo.m_addressModeV = SamplerAddressMode::CLAMP_TO_EDGE;
		samplerCreateInfo.m_addressModeW = SamplerAddressMode::CLAMP_TO_EDGE;

		m_graphicsDevice->createSampler(samplerCreateInfo, &m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX]);

		// repeat
		samplerCreateInfo.m_addressModeU = SamplerAddressMode::REPEAT;
		samplerCreateInfo.m_addressModeV = SamplerAddressMode::REPEAT;
		samplerCreateInfo.m_addressModeW = SamplerAddressMode::REPEAT;

		m_graphicsDevice->createSampler(samplerCreateInfo, &m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX]);
	}

	// point sampler
	{
		SamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.m_magFilter = Filter::NEAREST;
		samplerCreateInfo.m_minFilter = Filter::NEAREST;
		samplerCreateInfo.m_mipmapMode = SamplerMipmapMode::NEAREST;
		samplerCreateInfo.m_mipLodBias = 0.0f;
		samplerCreateInfo.m_anisotropyEnable = true;
		samplerCreateInfo.m_maxAnisotropy = m_graphicsDevice->getMaxSamplerAnisotropy();
		samplerCreateInfo.m_compareEnable = false;
		samplerCreateInfo.m_compareOp = CompareOp::NEVER;
		samplerCreateInfo.m_minLod = 0.0f;
		samplerCreateInfo.m_maxLod = 14.0f;
		samplerCreateInfo.m_borderColor = BorderColor::FLOAT_OPAQUE_WHITE;
		samplerCreateInfo.m_unnormalizedCoordinates = false;

		// clamp
		samplerCreateInfo.m_addressModeU = SamplerAddressMode::CLAMP_TO_EDGE;
		samplerCreateInfo.m_addressModeV = SamplerAddressMode::CLAMP_TO_EDGE;
		samplerCreateInfo.m_addressModeW = SamplerAddressMode::CLAMP_TO_EDGE;

		m_graphicsDevice->createSampler(samplerCreateInfo, &m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX]);

		// repeat
		samplerCreateInfo.m_addressModeU = SamplerAddressMode::REPEAT;
		samplerCreateInfo.m_addressModeV = SamplerAddressMode::REPEAT;
		samplerCreateInfo.m_addressModeW = SamplerAddressMode::REPEAT;

		m_graphicsDevice->createSampler(samplerCreateInfo, &m_samplers[RendererConsts::SAMPLER_POINT_REPEAT_IDX]);
	}

	// create texture descriptor pool
	{
		uint32_t typeCounts[(size_t)DescriptorType::RANGE_SIZE] = {};
		typeCounts[static_cast<size_t>(DescriptorType::SAMPLED_IMAGE)] = RendererConsts::TEXTURE_ARRAY_SIZE * 2 + 1 /*ImGui Fonts texture*/;
		typeCounts[static_cast<size_t>(DescriptorType::SAMPLER)] = 4 * 3;

		m_graphicsDevice->createDescriptorPool(3, typeCounts, &m_textureDescriptorSetPool);
	}

	// create texture descriptor set
	{
		DescriptorSetLayoutBinding bindings[]
		{
			{0, DescriptorType::SAMPLED_IMAGE, RendererConsts::TEXTURE_ARRAY_SIZE, ShaderStageFlagBits::FRAGMENT_BIT},
			{1, DescriptorType::SAMPLER, 4, ShaderStageFlagBits::FRAGMENT_BIT},
		};
		
		m_graphicsDevice->createDescriptorSetLayout(2, bindings, &m_textureDescriptorSetLayout);
		m_graphicsDevice->setDebugObjectName(ObjectType::DESCRIPTOR_SET_LAYOUT, m_textureDescriptorSetLayout, "Texture DSet Layout");
		m_textureDescriptorSetPool->allocateDescriptorSets(1, &m_textureDescriptorSetLayout, &m_textureDescriptorSet);
	}

	// create compute texture descriptor set
	{
		DescriptorSetLayoutBinding bindings[]
		{
			{0, DescriptorType::SAMPLED_IMAGE, RendererConsts::TEXTURE_ARRAY_SIZE, ShaderStageFlagBits::COMPUTE_BIT},
			{1, DescriptorType::SAMPLER, 4, ShaderStageFlagBits::COMPUTE_BIT},
		};
		
		m_graphicsDevice->createDescriptorSetLayout(2, bindings, &m_computeTextureDescriptorSetLayout);
		m_graphicsDevice->setDebugObjectName(ObjectType::DESCRIPTOR_SET_LAYOUT, m_textureDescriptorSetLayout, "Texture DSet Layout (Compute)");
		m_textureDescriptorSetPool->allocateDescriptorSets(1, &m_computeTextureDescriptorSetLayout, &m_computeTextureDescriptorSet);
	}

	// create ImGui descriptor set
	{
		DescriptorSetLayoutBinding bindings[]
		{
			{0, DescriptorType::SAMPLED_IMAGE, 1, ShaderStageFlagBits::FRAGMENT_BIT},
			{1, DescriptorType::SAMPLER, 1, ShaderStageFlagBits::FRAGMENT_BIT},
		};
		
		m_graphicsDevice->createDescriptorSetLayout(2, bindings, &m_imGuiDescriptorSetLayout);
		m_graphicsDevice->setDebugObjectName(ObjectType::DESCRIPTOR_SET_LAYOUT, m_textureDescriptorSetLayout, "ImGui DSet Layout");
		m_textureDescriptorSetPool->allocateDescriptorSets(1, &m_imGuiDescriptorSetLayout, &m_imGuiDescriptorSet);

		DescriptorSetUpdate update = Initializers::samplerDescriptor(&m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], 1);

		m_imGuiDescriptorSet->update(1, &update);
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

		BufferCreateInfo vertexBufferInfo{};
		vertexBufferInfo.m_size = sizeof(vertexData);
		vertexBufferInfo.m_createFlags = 0;
		vertexBufferInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::VERTEX_BUFFER_BIT;

		m_graphicsDevice->createBuffer(vertexBufferInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_lightProxyVertexBuffer);

		BufferCreateInfo indexBufferInfo{};
		indexBufferInfo.m_size = sizeof(indexData);
		indexBufferInfo.m_createFlags = 0;
		indexBufferInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::INDEX_BUFFER_BIT;

		m_graphicsDevice->createBuffer(vertexBufferInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_lightProxyIndexBuffer);

		void *data;
		m_stagingBuffer->map(&data);
		memcpy(data, vertexData, sizeof(vertexData));
		memcpy(((unsigned char *)data) + sizeof(vertexData), indexData, sizeof(indexData));
		m_stagingBuffer->unmap();


		m_commandListPool->reset();
		m_commandList->begin();
		{
			BufferCopy copyRegionVertex = { 0, 0, sizeof(vertexData) };
			m_commandList->copyBuffer(m_stagingBuffer, m_lightProxyVertexBuffer, 1, &copyRegionVertex);
			BufferCopy copyRegionIndex = { sizeof(vertexData), 0, sizeof(indexData) };
			m_commandList->copyBuffer(m_stagingBuffer, m_lightProxyIndexBuffer, 1, &copyRegionIndex);
		}
		m_commandList->end();
		Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), m_commandList);
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

		BufferCreateInfo indexBufferInfo{};
		indexBufferInfo.m_size = sizeof(indices);
		indexBufferInfo.m_createFlags = 0;
		indexBufferInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::INDEX_BUFFER_BIT;

		m_graphicsDevice->createBuffer(indexBufferInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_boxIndexBuffer);

		void *data;
		m_stagingBuffer->map(&data);
		memcpy(data, indices, sizeof(indices));
		m_stagingBuffer->unmap();

		m_commandListPool->reset();
		m_commandList->begin();
		{
			BufferCopy copyRegionIndex = { 0, 0, sizeof(indices) };
			m_commandList->copyBuffer(m_stagingBuffer, m_boxIndexBuffer, 1, &copyRegionIndex);
		}
		m_commandList->end();
		Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), m_commandList);
	}
}

void VEngine::RenderResources::resize(uint32_t width, uint32_t height)
{
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_depthImageState[i] = {};
		m_taaHistoryTextureState[i] = {};

		for (size_t j = 0; j < 14; ++j)
		{
			m_lightImageState[i][j] = {};
		}
	}

	// depth images
	{
		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = width;
		imageCreateInfo.m_height = height;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = Format::D32_SFLOAT;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT_BIT | ImageUsageFlagBits::SAMPLED_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			if (m_depthImages[i])
			{
				m_graphicsDevice->destroyImage(m_depthImages[i]);
			}
			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_depthImages[i]);
		}
	}

	// light images
	{
		uint32_t mipCount = 1 + static_cast<uint32_t>(glm::floor(glm::log2(float(glm::max(width, height)))));

		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = width;
		imageCreateInfo.m_height = height;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_levels = mipCount;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::STORAGE_BIT | ImageUsageFlagBits::SAMPLED_BIT | ImageUsageFlagBits::TRANSFER_SRC_BIT | ImageUsageFlagBits::TRANSFER_DST_BIT | ImageUsageFlagBits::COLOR_ATTACHMENT_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			if (m_lightImages[i])
			{
				m_graphicsDevice->destroyImage(m_lightImages[i]);
			}
			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_lightImages[i]);
		}
	}

	// TAA history textures
	{
		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = width;
		imageCreateInfo.m_height = height;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = Format::R16G16B16A16_SFLOAT;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::STORAGE_BIT | ImageUsageFlagBits::SAMPLED_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			if (m_taaHistoryTextures[i])
			{
				m_graphicsDevice->destroyImage(m_taaHistoryTextures[i]);
			}
			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_taaHistoryTextures[i]);
		}
	}
}

void VEngine::RenderResources::updateTextureArray(uint32_t count, gal::ImageView **data)
{
	DescriptorSetUpdate imageUpdate{};
	imageUpdate.m_dstBinding = 0;
	imageUpdate.m_dstArrayElement = 0;
	imageUpdate.m_descriptorCount = count < RendererConsts::TEXTURE_ARRAY_SIZE ? static_cast<uint32_t>(count) : RendererConsts::TEXTURE_ARRAY_SIZE;
	imageUpdate.m_descriptorType = DescriptorType::SAMPLED_IMAGE;
	imageUpdate.m_imageViews = data;

	DescriptorSetUpdate samplerUpdate{};
	samplerUpdate.m_dstBinding = 1;
	samplerUpdate.m_dstArrayElement = 0;
	samplerUpdate.m_descriptorCount = 4;
	samplerUpdate.m_descriptorType = DescriptorType::SAMPLER;
	samplerUpdate.m_samplers = m_samplers;

	DescriptorSetUpdate updates[] = { imageUpdate , samplerUpdate };

	m_textureDescriptorSet->update(2, updates);
	m_computeTextureDescriptorSet->update(2, updates);
}

void VEngine::RenderResources::createImGuiFontsTexture()
{
	ImGuiIO &io = ImGui::GetIO();

	unsigned char *pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	size_t upload_size = width * height * 4 * sizeof(char);

	assert(upload_size <= m_stagingBuffer->getDescription().m_size);

	// create image and view
	{
		// create image
		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = width;
		imageCreateInfo.m_height = height;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = Format::R8G8B8A8_UNORM;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::TRANSFER_DST_BIT | ImageUsageFlagBits::SAMPLED_BIT;

		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_imGuiFontsTexture);

		// create view
		m_graphicsDevice->createImageView(m_imGuiFontsTexture, &m_imGuiFontsTextureView);
	}

	// Update the Descriptor Set:
	{
		DescriptorSetUpdate update{};
		update.m_dstBinding = 0;
		update.m_dstArrayElement = 0;
		update.m_descriptorCount = 1;
		update.m_descriptorType = DescriptorType::SAMPLED_IMAGE;
		update.m_imageViews = &m_imGuiFontsTextureView;

		m_imGuiDescriptorSet->update(1, &update);
	}

	// Upload to Buffer:
	{
		uint8_t *map = nullptr;
		m_stagingBuffer->map((void **)&map);
		{
			memcpy(map, pixels, upload_size);
		}
		m_stagingBuffer->unmap();
	}

	// Copy to Image:
	{
		m_commandListPool->reset();
		m_commandList->begin();
		{
			// transition from UNDEFINED to TRANSFER_DST
			Barrier b0 = Initializers::imageBarrier(m_imGuiFontsTexture, PipelineStageFlagBits::HOST_BIT, PipelineStageFlagBits::TRANSFER_BIT, ResourceState::UNDEFINED, ResourceState::WRITE_IMAGE_TRANSFER);
			m_commandList->barrier(1, &b0);

			BufferImageCopy region{};
			region.m_imageLayerCount = 1;
			region.m_extent.m_width = width;
			region.m_extent.m_height = height;
			region.m_extent.m_depth = 1;

			m_commandList->copyBufferToImage(m_stagingBuffer, m_imGuiFontsTexture, 1, &region);
			
			// transition from TRANSFER_DST to TEXTURE
			Barrier b1 = Initializers::imageBarrier(m_imGuiFontsTexture, PipelineStageFlagBits::TRANSFER_BIT, PipelineStageFlagBits::FRAGMENT_SHADER_BIT, ResourceState::WRITE_IMAGE_TRANSFER, ResourceState::READ_TEXTURE);
			m_commandList->barrier(1, &b1);
		}
		m_commandList->end();
		Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), m_commandList);
	}

	// Store our identifier
	io.Fonts->TexID = (ImTextureID)m_imGuiFontsTexture;
}

void VEngine::RenderResources::setBVH(uint32_t nodeCount, const BVHNode *nodes, uint32_t triangleCount, const Triangle *triangles)
{
}
