#include "RenderResources.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "RenderData.h"
#include "Mesh.h"
#include "gal/Initializers.h"
#include "imgui/imgui.h"
#include "ProxyMeshes.h"

using namespace VEngine::gal;

VEngine::RenderResources::RenderResources(gal::GraphicsDevice *graphicsDevice)
	:m_graphicsDevice(graphicsDevice),
	m_pointLightProxyMeshIndexCount(PointLightProxyMesh::indexCount),
	m_pointLightProxyMeshFirstIndex(0),
	m_pointLightProxyMeshVertexOffset(0),
	m_spotLightProxyMeshIndexCount(SpotLightProxyMesh::indexCount),
	m_spotLightProxyMeshFirstIndex(PointLightProxyMesh::indexCount),
	m_spotLightProxyMeshVertexOffset(PointLightProxyMesh::vertexDataSize / (3 * sizeof(float))),
	m_boxProxyMeshIndexCount(BoxProxyMesh::indexCount),
	m_boxProxyMeshFirstIndex(PointLightProxyMesh::indexCount + SpotLightProxyMesh::indexCount),
	m_boxProxyMeshVertexOffset((PointLightProxyMesh::vertexDataSize + SpotLightProxyMesh::vertexDataSize) / (3 * sizeof(float)))
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
	m_graphicsDevice->destroyImage(m_shadowAtlasImage);
	m_graphicsDevice->destroyImage(m_imGuiFontsTexture);
	m_graphicsDevice->destroyImage(m_brdfLUT);

	// views
	m_graphicsDevice->destroyImageView(m_imGuiFontsTextureView);

	// buffers
	m_graphicsDevice->destroyBuffer(m_lightProxyVertexBuffer);
	m_graphicsDevice->destroyBuffer(m_lightProxyIndexBuffer);
	m_graphicsDevice->destroyBuffer(m_avgLuminanceBuffer);
	m_graphicsDevice->destroyBuffer(m_exposureDataBuffer);
	
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

	m_graphicsDevice->destroyDescriptorSetPool(m_textureDescriptorSetPool);
	m_graphicsDevice->destroyDescriptorSetPool(m_computeTextureDescriptorSetPool);
	m_graphicsDevice->destroyDescriptorSetPool(m_computeTexture3DDescriptorSetPool);
	
	m_graphicsDevice->destroyDescriptorSetLayout(m_textureDescriptorSetLayout);
	m_graphicsDevice->destroyDescriptorSetLayout(m_computeTextureDescriptorSetLayout);
	m_graphicsDevice->destroyDescriptorSetLayout(m_computeTexture3DDescriptorSetLayout);
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

	// shadow atlas
	{
		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = 8192;
		imageCreateInfo.m_height = 8192;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_layers = 1;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_format = Format::D32_SFLOAT;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::SAMPLED_BIT | ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT_BIT;

		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, true, &m_shadowAtlasImage);
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
				m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::HOST_VISIBLE_BIT | MemoryPropertyFlagBits::HOST_COHERENT_BIT, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, true, &m_uboBuffers[i]);
				m_mappableUBOBlock[i] = std::make_unique<MappableBufferBlock>(m_uboBuffers[i], m_graphicsDevice->getMinUniformBufferOffsetAlignment());
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
				m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::HOST_VISIBLE_BIT | MemoryPropertyFlagBits::HOST_COHERENT_BIT, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, true, &m_ssboBuffers[i]);
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

	// exposure data buffer
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = sizeof(float) * 2;
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::STORAGE_BUFFER_BIT | BufferUsageFlagBits::TRANSFER_DST_BIT;

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_exposureDataBuffer);

		m_commandListPool->reset();
		m_commandList->begin();
		{
			float data[] = { 1.0f, 1.0f };
			m_commandList->updateBuffer(m_exposureDataBuffer, 0, 8, data);
			m_exposureDataBufferState.m_queue = m_graphicsDevice->getGraphicsQueue();
			m_exposureDataBufferState.m_stateStageMask = { ResourceState::WRITE_BUFFER_TRANSFER, PipelineStageFlagBits::TRANSFER_BIT };
		}
		m_commandList->end();
		Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), m_commandList);
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

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, true, &m_vertexBuffer);
	}

	// index buffer
	{
		BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.m_size = RendererConsts::MAX_INDICES * sizeof(uint16_t);
		bufferCreateInfo.m_createFlags = 0;
		bufferCreateInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::INDEX_BUFFER_BIT | BufferUsageFlagBits::STORAGE_BUFFER_BIT;

		m_graphicsDevice->createBuffer(bufferCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, true, &m_indexBuffer);
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
		samplerCreateInfo.m_compareOp = CompareOp::LESS_OR_EQUAL;
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

	// create texture descriptor set
	{
		DescriptorSetLayoutBinding bindings[]
		{
			{0, DescriptorType::SAMPLED_IMAGE, RendererConsts::TEXTURE_ARRAY_SIZE, ShaderStageFlagBits::FRAGMENT_BIT},
			{1, DescriptorType::SAMPLER, 4, ShaderStageFlagBits::FRAGMENT_BIT},
		};
		
		m_graphicsDevice->createDescriptorSetLayout(2, bindings, &m_textureDescriptorSetLayout);
		m_graphicsDevice->setDebugObjectName(ObjectType::DESCRIPTOR_SET_LAYOUT, m_textureDescriptorSetLayout, "Texture DSet Layout");
		m_graphicsDevice->createDescriptorSetPool(1, m_textureDescriptorSetLayout, &m_textureDescriptorSetPool);
		m_textureDescriptorSetPool->allocateDescriptorSets(1, &m_textureDescriptorSet);
	}

	// create compute texture descriptor set
	{
		DescriptorSetLayoutBinding bindings[]
		{
			{0, DescriptorType::SAMPLED_IMAGE, RendererConsts::TEXTURE_ARRAY_SIZE, ShaderStageFlagBits::COMPUTE_BIT},
			{1, DescriptorType::SAMPLER, 4, ShaderStageFlagBits::COMPUTE_BIT},
		};
		
		m_graphicsDevice->createDescriptorSetLayout(2, bindings, &m_computeTextureDescriptorSetLayout);
		m_graphicsDevice->setDebugObjectName(ObjectType::DESCRIPTOR_SET_LAYOUT, m_computeTextureDescriptorSetLayout, "Texture DSet Layout (Compute)");
		m_graphicsDevice->createDescriptorSetPool(1, m_computeTextureDescriptorSetLayout, &m_computeTextureDescriptorSetPool);
		m_computeTextureDescriptorSetPool->allocateDescriptorSets(1, &m_computeTextureDescriptorSet);
	}

	// create compute texture3d descriptor set
	{
		DescriptorSetLayoutBinding bindings[]
		{
			{0, DescriptorType::SAMPLED_IMAGE, RendererConsts::TEXTURE_ARRAY_SIZE, ShaderStageFlagBits::COMPUTE_BIT},
		};

		m_graphicsDevice->createDescriptorSetLayout(1, bindings, &m_computeTexture3DDescriptorSetLayout);
		m_graphicsDevice->setDebugObjectName(ObjectType::DESCRIPTOR_SET_LAYOUT, m_computeTexture3DDescriptorSetLayout, "Texture DSet Layout (Compute)");
		m_graphicsDevice->createDescriptorSetPool(1, m_computeTexture3DDescriptorSetLayout, &m_computeTexture3DDescriptorSetPool);
		m_computeTexture3DDescriptorSetPool->allocateDescriptorSets(1, &m_computeTexture3DDescriptorSet);
	}

	createImGuiFontsTexture();

	// light proxy vertex/index buffer
	{
		BufferCreateInfo vertexBufferInfo{};
		vertexBufferInfo.m_size = PointLightProxyMesh::vertexDataSize + SpotLightProxyMesh::vertexDataSize + BoxProxyMesh::vertexDataSize;
		vertexBufferInfo.m_createFlags = 0;
		vertexBufferInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::VERTEX_BUFFER_BIT;

		m_graphicsDevice->createBuffer(vertexBufferInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_lightProxyVertexBuffer);

		BufferCreateInfo indexBufferInfo{};
		indexBufferInfo.m_size = PointLightProxyMesh::indexDataSize + SpotLightProxyMesh::indexDataSize + BoxProxyMesh::indexDataSize;
		indexBufferInfo.m_createFlags = 0;
		indexBufferInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT | BufferUsageFlagBits::INDEX_BUFFER_BIT;

		m_graphicsDevice->createBuffer(indexBufferInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_lightProxyIndexBuffer);

		uint8_t *data;
		size_t srcOffset = 0;
		size_t vertexDstOffset = 0;
		size_t indexDstOffset = 0;

		m_stagingBuffer->map((void **)&data);

		// point light vertex buffer
		memcpy(data + srcOffset, PointLightProxyMesh::vertexData, PointLightProxyMesh::vertexDataSize);
		BufferCopy copyRegionPointLightVertex = { srcOffset, vertexDstOffset, PointLightProxyMesh::vertexDataSize };
		srcOffset += PointLightProxyMesh::vertexDataSize;
		vertexDstOffset += PointLightProxyMesh::vertexDataSize;

		// spot light vertex buffer
		memcpy(data + srcOffset, SpotLightProxyMesh::vertexData, SpotLightProxyMesh::vertexDataSize);
		BufferCopy copyRegionSpotLightVertex = { srcOffset, vertexDstOffset, SpotLightProxyMesh::vertexDataSize };
		srcOffset += SpotLightProxyMesh::vertexDataSize;
		vertexDstOffset += SpotLightProxyMesh::vertexDataSize;

		// box vertex buffer
		memcpy(data + srcOffset, BoxProxyMesh::vertexData, BoxProxyMesh::vertexDataSize);
		BufferCopy copyRegionBoxVertex = { srcOffset, vertexDstOffset, BoxProxyMesh::vertexDataSize };
		srcOffset += BoxProxyMesh::vertexDataSize;
		vertexDstOffset += BoxProxyMesh::vertexDataSize;

		// point light index buffer
		memcpy(data + srcOffset, PointLightProxyMesh::indexData, PointLightProxyMesh::indexDataSize);
		BufferCopy copyRegionPointLightIndex = { srcOffset, indexDstOffset, PointLightProxyMesh::indexDataSize };
		srcOffset += PointLightProxyMesh::indexDataSize;
		indexDstOffset += PointLightProxyMesh::indexDataSize;

		// spot light index buffer
		memcpy(data + srcOffset, SpotLightProxyMesh::indexData, SpotLightProxyMesh::indexDataSize);
		BufferCopy copyRegionSpotLightIndex = { srcOffset, indexDstOffset, SpotLightProxyMesh::indexDataSize };
		srcOffset += SpotLightProxyMesh::indexDataSize;
		indexDstOffset += SpotLightProxyMesh::indexDataSize;

		// box index buffer
		memcpy(data + srcOffset, BoxProxyMesh::indexData, BoxProxyMesh::indexDataSize);
		BufferCopy copyRegionBoxIndex = { srcOffset, indexDstOffset, BoxProxyMesh::indexDataSize };
		srcOffset += BoxProxyMesh::indexDataSize;
		indexDstOffset += BoxProxyMesh::indexDataSize;

		m_stagingBuffer->unmap();


		m_commandListPool->reset();
		m_commandList->begin();
		{
			BufferCopy vertexCopies[] = { copyRegionPointLightVertex, copyRegionSpotLightVertex, copyRegionBoxVertex };
			m_commandList->copyBuffer(m_stagingBuffer, m_lightProxyVertexBuffer, 3, vertexCopies);

			BufferCopy indexCopies[] = { copyRegionPointLightIndex, copyRegionSpotLightIndex, copyRegionBoxIndex };
			m_commandList->copyBuffer(m_stagingBuffer, m_lightProxyIndexBuffer, 3, indexCopies);
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
			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, true, &m_depthImages[i]);
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
		imageCreateInfo.m_format = Format::B10G11R11_UFLOAT_PACK32;
		imageCreateInfo.m_createFlags = 0;
		imageCreateInfo.m_usageFlags = ImageUsageFlagBits::STORAGE_BIT | ImageUsageFlagBits::SAMPLED_BIT | ImageUsageFlagBits::TRANSFER_SRC_BIT | ImageUsageFlagBits::TRANSFER_DST_BIT | ImageUsageFlagBits::COLOR_ATTACHMENT_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			if (m_lightImages[i])
			{
				m_graphicsDevice->destroyImage(m_lightImages[i]);
			}
			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, true, &m_lightImages[i]);
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
			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, true, &m_taaHistoryTextures[i]);
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

void VEngine::RenderResources::updateTexture3DArray(uint32_t count, gal::ImageView **data)
{
	DescriptorSetUpdate imageUpdate{};
	imageUpdate.m_dstBinding = 0;
	imageUpdate.m_dstArrayElement = 0;
	imageUpdate.m_descriptorCount = count < RendererConsts::TEXTURE_ARRAY_SIZE ? static_cast<uint32_t>(count) : RendererConsts::TEXTURE_ARRAY_SIZE;
	imageUpdate.m_descriptorType = DescriptorType::SAMPLED_IMAGE;
	imageUpdate.m_imageViews = data;

	m_computeTexture3DDescriptorSet->update(1, &imageUpdate);
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
	//io.Fonts->TexID = (ImTextureID)m_imGuiFontsTexture;
}

void VEngine::RenderResources::setBVH(uint32_t nodeCount, const BVHNode *nodes, uint32_t triangleCount, const Triangle *triangles)
{
}
