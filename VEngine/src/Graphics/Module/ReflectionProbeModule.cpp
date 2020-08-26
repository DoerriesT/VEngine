#include "ReflectionProbeModule.h"
#include "Graphics/gal/Initializers.h"
#include "Graphics/RenderResources.h"
#include "Graphics/Pass/ShadowPass.h"
#include "Graphics/Pass/ProbeGBufferPass.h"
#include "Graphics/Pass/LightProbeGBufferPass.h"
#include "Graphics/Pass/ProbeDownsamplePass.h"
#include "Graphics/Pass/ProbeDownsamplePass2.h"
#include "Graphics/Pass/ProbeFilterPass.h"
#include "Graphics/Pass/ProbeFilterImportanceSamplingPass.h"
#include "Graphics/Pass/ProbeCompressBCH6Pass.h"
#include "Graphics/LightData.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/ViewRenderList.h"
#include <glm/vec4.hpp>
#include "Utility/Utility.h"

namespace
{
	using float4 = glm::vec4;
#include "../../../../Application/Resources/Shaders/hlsl/src/probeFilterCoeffsQuad32.hlsli"
}

using namespace VEngine::gal;

VEngine::ReflectionProbeModule::ReflectionProbeModule(gal::GraphicsDevice *graphicsDevice, RenderResources *renderResources)
	:m_graphicsDevice(graphicsDevice)
{
	// gbuffer images
	{
		ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.m_width = RendererConsts::REFLECTION_PROBE_RES;
		imageCreateInfo.m_height = RendererConsts::REFLECTION_PROBE_RES;
		imageCreateInfo.m_depth = 1;
		imageCreateInfo.m_levels = 1;
		imageCreateInfo.m_layers = 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE;
		imageCreateInfo.m_samples = SampleCount::_1;
		imageCreateInfo.m_imageType = ImageType::_2D;
		imageCreateInfo.m_createFlags = 0;// ImageCreateFlagBits::CUBE_COMPATIBLE_BIT;

		// depth
		{
			imageCreateInfo.m_format = Format::D16_UNORM;
			imageCreateInfo.m_usageFlags = ImageUsageFlagBits::TEXTURE_BIT | ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT_BIT;

			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_probeDepthArrayImage);

			// array view
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeDepthArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = 0;
				viewCreateInfo.m_layerCount = 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE;

				m_graphicsDevice->createImageView(viewCreateInfo, &m_probeDepthArrayView);
			}

			// slice views
			for (uint32_t i = 0; i < 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE; ++i)
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeDepthArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = i;
				viewCreateInfo.m_layerCount = 1;

				m_graphicsDevice->createImageView(viewCreateInfo, &m_probeDepthSliceViews[i]);
			}
		}

		// albedo
		{
			imageCreateInfo.m_format = Format::R8G8B8A8_UNORM;
			imageCreateInfo.m_usageFlags = ImageUsageFlagBits::TEXTURE_BIT | ImageUsageFlagBits::COLOR_ATTACHMENT_BIT;

			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_probeAlbedoRoughnessArrayImage);

			// array view
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeAlbedoRoughnessArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = 0;
				viewCreateInfo.m_layerCount = 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE;

				m_graphicsDevice->createImageView(viewCreateInfo, &m_probeAlbedoRoughnessArrayView);
			}

			// slice views
			for (uint32_t i = 0; i < 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE; ++i)
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeAlbedoRoughnessArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = i;
				viewCreateInfo.m_layerCount = 1;

				m_graphicsDevice->createImageView(viewCreateInfo, &m_probeAlbedoRoughnessSliceViews[i]);
			}
		}

		// normal
		{
			imageCreateInfo.m_format = Format::R16G16_SFLOAT;
			imageCreateInfo.m_usageFlags = ImageUsageFlagBits::TEXTURE_BIT | ImageUsageFlagBits::COLOR_ATTACHMENT_BIT;

			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_probeNormalArrayImage);

			// array view
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeNormalArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = 0;
				viewCreateInfo.m_layerCount = 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE;

				m_graphicsDevice->createImageView(viewCreateInfo, &m_probeNormalArrayView);
			}

			// slice views
			for (uint32_t i = 0; i < 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE; ++i)
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeNormalArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D;
				viewCreateInfo.m_format = imageCreateInfo.m_format;
				viewCreateInfo.m_baseMipLevel = 0;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = i;
				viewCreateInfo.m_layerCount = 1;

				m_graphicsDevice->createImageView(viewCreateInfo, &m_probeNormalSliceViews[i]);
			}
		}
	}

	// uncompressed lit image -> result of filtering
	//{
	//	ImageCreateInfo imageCreateInfo{};
	//	imageCreateInfo.m_width = RendererConsts::REFLECTION_PROBE_RES;
	//	imageCreateInfo.m_height = RendererConsts::REFLECTION_PROBE_RES;
	//	imageCreateInfo.m_depth = 1;
	//	imageCreateInfo.m_levels = RendererConsts::REFLECTION_PROBE_MIPS;
	//	imageCreateInfo.m_layers = 6;
	//	imageCreateInfo.m_samples = SampleCount::_1;
	//	imageCreateInfo.m_imageType = ImageType::_2D;
	//	imageCreateInfo.m_format = Format::B10G11R11_UFLOAT_PACK32;
	//	imageCreateInfo.m_createFlags = 0;
	//	imageCreateInfo.m_usageFlags = ImageUsageFlagBits::SAMPLED_BIT | ImageUsageFlagBits::STORAGE_BIT;
	//
	//	m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_probeUncompressedLitImage);
	//
	//	for (uint32_t i = 0; i < RendererConsts::REFLECTION_PROBE_MIPS; ++i)
	//	{
	//		gal::ImageViewCreateInfo viewCreateInfo{};
	//		viewCreateInfo.m_image = m_probeUncompressedLitImage;
	//		viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
	//		viewCreateInfo.m_format = imageCreateInfo.m_format;
	//		viewCreateInfo.m_baseMipLevel = i;
	//		viewCreateInfo.m_levelCount = 1;
	//		viewCreateInfo.m_baseArrayLayer = 0;
	//		viewCreateInfo.m_layerCount = 6;
	//
	//		m_graphicsDevice->createImageView(viewCreateInfo, &m_probeUncompressedMipViews[i]);
	//	}
	//}

	// compressed uint tmp -> result of compression
	//{
	//	ImageCreateInfo createInfo{};
	//	createInfo.m_width = RendererConsts::REFLECTION_PROBE_RES / 4;
	//	createInfo.m_height = RendererConsts::REFLECTION_PROBE_RES / 4;
	//	createInfo.m_depth = 1;
	//	createInfo.m_levels = RendererConsts::REFLECTION_PROBE_MIPS;
	//	createInfo.m_layers = 6;
	//	createInfo.m_samples = SampleCount::_1;
	//	createInfo.m_imageType = ImageType::_2D;
	//	createInfo.m_format = Format::R32G32B32A32_UINT;
	//	createInfo.m_createFlags = 0;
	//	createInfo.m_usageFlags = ImageUsageFlagBits::TRANSFER_SRC_BIT | ImageUsageFlagBits::STORAGE_BIT;
	//
	//	m_graphicsDevice->createImage(createInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_probeCompressedTmpLitImage);
	//
	//	// mip views
	//	for (size_t j = 0; j < RendererConsts::REFLECTION_PROBE_MIPS; ++j)
	//	{
	//		gal::ImageViewCreateInfo viewCreateInfo{};
	//		viewCreateInfo.m_image = m_probeCompressedTmpLitImage;
	//		viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
	//		viewCreateInfo.m_format = createInfo.m_format;
	//		viewCreateInfo.m_baseMipLevel = j;
	//		viewCreateInfo.m_levelCount = 1;
	//		viewCreateInfo.m_baseArrayLayer = 0;
	//		viewCreateInfo.m_layerCount = 6;
	//
	//		m_graphicsDevice->createImageView(viewCreateInfo, &m_probeCompressedTmpMipViews[j]);
	//	}
	//}

	// lit array -> sampled during shading
	{
		ImageCreateInfo createInfo{};
		createInfo.m_width = RendererConsts::REFLECTION_PROBE_RES;
		createInfo.m_height = RendererConsts::REFLECTION_PROBE_RES;
		createInfo.m_depth = 1;
		createInfo.m_levels = RendererConsts::REFLECTION_PROBE_MIPS;
		createInfo.m_layers = 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE;
		createInfo.m_samples = SampleCount::_1;
		createInfo.m_imageType = ImageType::_2D;
		createInfo.m_format = Format::B10G11R11_UFLOAT_PACK32;
		createInfo.m_createFlags = ImageCreateFlagBits::CUBE_COMPATIBLE_BIT;
		createInfo.m_usageFlags = ImageUsageFlagBits::TEXTURE_BIT | ImageUsageFlagBits::RW_TEXTURE_BIT;

		m_graphicsDevice->createImage(createInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_probeArrayImage);

		// cube array view
		{
			gal::ImageViewCreateInfo viewCreateInfo{};
			viewCreateInfo.m_image = m_probeArrayImage;
			viewCreateInfo.m_viewType = gal::ImageViewType::CUBE_ARRAY;
			viewCreateInfo.m_format = createInfo.m_format;
			viewCreateInfo.m_baseMipLevel = 0;
			viewCreateInfo.m_levelCount = RendererConsts::REFLECTION_PROBE_MIPS;
			viewCreateInfo.m_baseArrayLayer = 0;
			viewCreateInfo.m_layerCount = 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE;

			m_graphicsDevice->createImageView(viewCreateInfo, &m_probeCubeArrayView);
		}

		// mip views
		for (size_t i = 0; i < RendererConsts::REFLECTION_PROBE_CACHE_SIZE; ++i)
		{
			for (size_t j = 0; j < RendererConsts::REFLECTION_PROBE_MIPS; ++j)
			{
				gal::ImageViewCreateInfo viewCreateInfo{};
				viewCreateInfo.m_image = m_probeArrayImage;
				viewCreateInfo.m_viewType = gal::ImageViewType::_2D_ARRAY;
				viewCreateInfo.m_format = createInfo.m_format;
				viewCreateInfo.m_baseMipLevel = j;
				viewCreateInfo.m_levelCount = 1;
				viewCreateInfo.m_baseArrayLayer = i * 6;
				viewCreateInfo.m_layerCount = 6;

				m_graphicsDevice->createImageView(viewCreateInfo, &m_probeMipViews[i][j]);
			}
		}
	}

	// transition images to expected layouts
	{
		auto *cmdList = renderResources->m_commandList;
		renderResources->m_commandListPool->reset();
		cmdList->begin();
		{
			Barrier barriers[]
			{
				Initializers::imageBarrier(m_probeDepthArrayImage, PipelineStageFlagBits::TOP_OF_PIPE_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_TEXTURE, {0, 1, 0, 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE}),
				Initializers::imageBarrier(m_probeAlbedoRoughnessArrayImage, PipelineStageFlagBits::TOP_OF_PIPE_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_TEXTURE, {0, 1, 0, 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE}),
				Initializers::imageBarrier(m_probeNormalArrayImage, PipelineStageFlagBits::TOP_OF_PIPE_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_TEXTURE, {0, 1, 0, 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE}),
				//Initializers::imageBarrier(m_probeUncompressedLitImage, PipelineStageFlagBits::TOP_OF_PIPE_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_TEXTURE, {0, RendererConsts::REFLECTION_PROBE_MIPS, 0, 6}),
				//Initializers::imageBarrier(m_probeCompressedTmpLitImage, PipelineStageFlagBits::TOP_OF_PIPE_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_IMAGE_TRANSFER, {0, RendererConsts::REFLECTION_PROBE_MIPS, 0, 6}),
				Initializers::imageBarrier(m_probeArrayImage, PipelineStageFlagBits::TOP_OF_PIPE_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT | PipelineStageFlagBits::FRAGMENT_SHADER_BIT, ResourceState::UNDEFINED, ResourceState::READ_TEXTURE, {0, RendererConsts::REFLECTION_PROBE_MIPS, 0, 6 * RendererConsts::REFLECTION_PROBE_CACHE_SIZE}),
			};
			cmdList->barrier(sizeof(barriers) / sizeof(barriers[0]), barriers);
		}
		cmdList->end();
		Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), cmdList);
	}

	// tmp image -> result of downsampling and input to filtering. is graph managed
	{
		ImageCreateInfo createInfo{};
		createInfo.m_width = RendererConsts::REFLECTION_PROBE_RES;
		createInfo.m_height = RendererConsts::REFLECTION_PROBE_RES;
		createInfo.m_depth = 1;
		createInfo.m_levels = RendererConsts::REFLECTION_PROBE_MIPS;
		createInfo.m_layers = 6;
		createInfo.m_samples = SampleCount::_1;
		createInfo.m_imageType = ImageType::_2D;
		createInfo.m_format = Format::B10G11R11_UFLOAT_PACK32;
		createInfo.m_createFlags = ImageCreateFlagBits::CUBE_COMPATIBLE_BIT;
		createInfo.m_usageFlags = ImageUsageFlagBits::TEXTURE_BIT | ImageUsageFlagBits::RW_TEXTURE_BIT;

		m_graphicsDevice->createImage(createInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_probeTmpImage);

		for (uint32_t i = 0; i < RendererConsts::REFLECTION_PROBE_MIPS; ++i)
		{
			gal::ImageViewCreateInfo viewCreateInfo{};
			viewCreateInfo.m_image = m_probeTmpImage;
			viewCreateInfo.m_viewType = gal::ImageViewType::CUBE;
			viewCreateInfo.m_format = createInfo.m_format;
			viewCreateInfo.m_baseMipLevel = i;
			viewCreateInfo.m_levelCount = 1;
			viewCreateInfo.m_baseArrayLayer = 0;
			viewCreateInfo.m_layerCount = 6;

			m_graphicsDevice->createImageView(viewCreateInfo, &m_probeTmpCubeViews[i]);
		}
	}

	// reflection probe filter coeffs image
	{
		// create image and view
		{
			// create image
			ImageCreateInfo imageCreateInfo{};
			imageCreateInfo.m_width = sizeof(coeffs) / 16;
			imageCreateInfo.m_height = 1;
			imageCreateInfo.m_depth = 1;
			imageCreateInfo.m_levels = 1;
			imageCreateInfo.m_layers = 1;
			imageCreateInfo.m_samples = SampleCount::_1;
			imageCreateInfo.m_imageType = ImageType::_1D;
			imageCreateInfo.m_format = Format::R32G32B32A32_SFLOAT;
			imageCreateInfo.m_createFlags = 0;
			imageCreateInfo.m_usageFlags = ImageUsageFlagBits::TRANSFER_DST_BIT | ImageUsageFlagBits::TEXTURE_BIT;
	
			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_probeFilterCoeffsImage);
	
			// create view
			m_graphicsDevice->createImageView(m_probeFilterCoeffsImage, &m_probeFilterCoeffsImageView);
		}
	
		auto *cmdList = renderResources->m_commandList;
	
		// Upload to Buffer:
		{
			uint8_t *map = nullptr;
			renderResources->m_stagingBuffer->map((void **)&map);
			{
				memcpy(map, coeffs, sizeof(coeffs));
			}
			renderResources->m_stagingBuffer->unmap();
		}
	
		// Copy to Image:
		{
			renderResources->m_commandListPool->reset();
			cmdList->begin();
			{
				// transition from UNDEFINED to TRANSFER_DST
				Barrier b0 = Initializers::imageBarrier(m_probeFilterCoeffsImage, PipelineStageFlagBits::HOST_BIT, PipelineStageFlagBits::TRANSFER_BIT, ResourceState::UNDEFINED, ResourceState::WRITE_IMAGE_TRANSFER);
				cmdList->barrier(1, &b0);
	
				BufferImageCopy region{};
				region.m_bufferOffset = 0;
				region.m_bufferRowLength = Utility::alignUp(sizeof(coeffs), m_graphicsDevice->getBufferCopyRowPitchAlignment()) / 16;
				region.m_bufferImageHeight = 1;
				region.m_imageLayerCount = 1;
				region.m_extent.m_width = sizeof(coeffs) / 16;
				region.m_extent.m_height = 1;
				region.m_extent.m_depth = 1;
	
				cmdList->copyBufferToImage(renderResources->m_stagingBuffer, m_probeFilterCoeffsImage, 1, &region);
	
				// transition from TRANSFER_DST to TEXTURE
				Barrier b1 = Initializers::imageBarrier(m_probeFilterCoeffsImage, PipelineStageFlagBits::TRANSFER_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT, ResourceState::WRITE_IMAGE_TRANSFER, ResourceState::READ_TEXTURE);
				cmdList->barrier(1, &b1);
			}
			cmdList->end();
			Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), cmdList);
		}
	}
}

VEngine::ReflectionProbeModule::~ReflectionProbeModule()
{
	m_graphicsDevice->destroyImage(m_probeDepthArrayImage);
	m_graphicsDevice->destroyImage(m_probeAlbedoRoughnessArrayImage);
	m_graphicsDevice->destroyImage(m_probeNormalArrayImage);
	m_graphicsDevice->destroyImage(m_probeArrayImage);
	m_graphicsDevice->destroyImage(m_probeTmpImage);
	m_graphicsDevice->destroyImage(m_probeFilterCoeffsImage);

	for (size_t i = 0; i < RendererConsts::REFLECTION_PROBE_MIPS; ++i)
	{
		m_graphicsDevice->destroyImageView(m_probeTmpCubeViews[i]);
	}
	m_graphicsDevice->destroyImageView(m_probeDepthArrayView);
	m_graphicsDevice->destroyImageView(m_probeAlbedoRoughnessArrayView);
	m_graphicsDevice->destroyImageView(m_probeNormalArrayView);
	for (size_t i = 0; i < RendererConsts::REFLECTION_PROBE_CACHE_SIZE * 6; ++i)
	{
		m_graphicsDevice->destroyImageView(m_probeDepthSliceViews[i]);
		m_graphicsDevice->destroyImageView(m_probeAlbedoRoughnessSliceViews[i]);
		m_graphicsDevice->destroyImageView(m_probeNormalSliceViews[i]);
	}

	m_graphicsDevice->destroyImageView(m_probeCubeArrayView);
	for (size_t i = 0; i < RendererConsts::REFLECTION_PROBE_CACHE_SIZE; ++i)
	{
		for (size_t j = 0; j < RendererConsts::REFLECTION_PROBE_MIPS; ++j)
		{
			m_graphicsDevice->destroyImageView(m_probeMipViews[i][j]);
		}
	}
	m_graphicsDevice->destroyImageView(m_probeFilterCoeffsImageView);
}

void VEngine::ReflectionProbeModule::addGBufferRenderingToGraph(rg::RenderGraph &graph, const GBufferRenderingData &data)
{
	auto *renderResources = data.m_passRecordContext->m_renderResources;
	const uint32_t *renderIndices = data.m_renderData->m_probeRenderIndices;
	for (size_t i = 0; i < data.m_renderData->m_probeRenderCount; ++i)
	{
		ProbeGBufferPass::Data probeGBufferPassData;
		probeGBufferPassData.m_passRecordContext = data.m_passRecordContext;
		memcpy(probeGBufferPassData.m_viewProjectionMatrices, data.m_renderData->m_probeViewProjectionMatrices + 6 * i, sizeof(glm::mat4) * 6);
		for (size_t j = 0; j < 6; ++j)
		{
			probeGBufferPassData.m_opaqueInstanceDataCount[j] = data.m_renderData->m_renderLists[data.m_renderData->m_probeDrawListOffset + 6 * i + j].m_opaqueCount;
			probeGBufferPassData.m_opaqueInstanceDataOffset[j] = data.m_renderData->m_renderLists[data.m_renderData->m_probeDrawListOffset + 6 * i + j].m_opaqueOffset;
			probeGBufferPassData.m_maskedInstanceDataCount[j] = data.m_renderData->m_renderLists[data.m_renderData->m_probeDrawListOffset + 6 * i + j].m_maskedCount;
			probeGBufferPassData.m_maskedInstanceDataOffset[j] = data.m_renderData->m_renderLists[data.m_renderData->m_probeDrawListOffset + 6 * i + j].m_maskedOffset;
			probeGBufferPassData.m_depthImageViews[j] = m_probeDepthSliceViews[j + renderIndices[i] * 6];
			probeGBufferPassData.m_albedoRoughnessImageViews[j] = m_probeAlbedoRoughnessSliceViews[j + renderIndices[i] * 6];
			probeGBufferPassData.m_normalImageViews[j] = m_probeNormalSliceViews[j + renderIndices[i] * 6];
		}
		probeGBufferPassData.m_instanceData = data.m_instanceData;
		probeGBufferPassData.m_subMeshInfo = data.m_subMeshInfo;
		probeGBufferPassData.m_texCoordScaleBias = &data.m_renderData->m_texCoordScaleBias[0].x;
		probeGBufferPassData.m_materialDataBufferInfo = { renderResources->m_materialBuffer, 0, renderResources->m_materialBuffer->getDescription().m_size, sizeof(MaterialData) };
		probeGBufferPassData.m_transformDataBufferInfo = data.m_transformDataBufferInfo;

		ProbeGBufferPass::addToGraph(graph, probeGBufferPassData);
	}
}

void VEngine::ReflectionProbeModule::addShadowRenderingToGraph(rg::RenderGraph &graph, const ShadowRenderingData &data)
{
	rg::ImageDescription desc = {};
	desc.m_name = "Probe Shadow Image";
	desc.m_clear = false;
	desc.m_clearValue.m_imageClearValue = {};
	desc.m_width = 2048;
	desc.m_height = 2048;
	desc.m_layers = glm::max(data.m_renderData->m_probeShadowViewRenderListCount, 1u);
	desc.m_levels = 1;
	desc.m_samples = SampleCount::_1;
	desc.m_format = Format::D16_UNORM;

	rg::ImageHandle shadowImageHandle = graph.createImage(desc);
	m_probeShadowImageViewHandle = graph.createImageView({ desc.m_name, shadowImageHandle, { 0, 1, 0, desc.m_layers }, ImageViewType::_2D_ARRAY });

	for (uint32_t i = 0; i < data.m_renderData->m_probeShadowViewRenderListCount; ++i)
	{
		rg::ImageViewHandle shadowLayer = graph.createImageView({ desc.m_name, shadowImageHandle, { 0, 1, i, 1 } });

		const auto &drawList = data.m_renderData->m_renderLists[data.m_renderData->m_probeShadowViewRenderListOffset + i];

		// draw shadows
		ShadowPass::Data shadowPassData;
		shadowPassData.m_passRecordContext = data.m_passRecordContext;
		shadowPassData.m_shadowMapSize = 2048;
		shadowPassData.m_shadowMatrix = data.m_renderData->m_shadowMatrices[data.m_directionalLightsShadowedProbe[i].m_shadowOffset];
		shadowPassData.m_opaqueInstanceDataCount = drawList.m_opaqueCount;
		shadowPassData.m_opaqueInstanceDataOffset = drawList.m_opaqueOffset;
		shadowPassData.m_maskedInstanceDataCount = drawList.m_maskedCount;
		shadowPassData.m_maskedInstanceDataOffset = drawList.m_maskedOffset;
		shadowPassData.m_instanceData = data.m_instanceData;
		shadowPassData.m_subMeshInfo = data.m_subMeshInfo;
		shadowPassData.m_texCoordScaleBias = &data.m_renderData->m_texCoordScaleBias[0].x;
		shadowPassData.m_materialDataBufferInfo = { data.m_passRecordContext->m_renderResources->m_materialBuffer, 0, data.m_passRecordContext->m_renderResources->m_materialBuffer->getDescription().m_size, sizeof(MaterialData) };
		shadowPassData.m_transformDataBufferInfo = data.m_transformDataBufferInfo;
		shadowPassData.m_shadowImageHandle = shadowLayer;

		ShadowPass::addToGraph(graph, shadowPassData);
	}
}

void VEngine::ReflectionProbeModule::addRelightingToGraph(rg::RenderGraph &graph, const RelightingData &data)
{
	// shadowed directional light probe data write
	DescriptorBufferInfo directionalLightsShadowedProbeBufferInfo{ nullptr, 0, std::max(data.m_lightData->m_directionalLightsShadowedProbe.size(), size_t(1)) * sizeof(DirectionalLight), sizeof(DirectionalLight) };
	{
		uint64_t alignment = m_graphicsDevice->getBufferAlignment(DescriptorType2::STRUCTURED_BUFFER, sizeof(DirectionalLight));
		uint8_t *bufferPtr;
		data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[data.m_passRecordContext->m_commonRenderData->m_curResIdx]->allocate(alignment, directionalLightsShadowedProbeBufferInfo.m_range, directionalLightsShadowedProbeBufferInfo.m_offset, directionalLightsShadowedProbeBufferInfo.m_buffer, bufferPtr);
		if (!data.m_lightData->m_directionalLightsShadowedProbe.empty())
		{
			memcpy(bufferPtr, data.m_lightData->m_directionalLightsShadowedProbe.data(), data.m_lightData->m_directionalLightsShadowedProbe.size() * sizeof(DirectionalLight));
		}
	}

	rg::ImageHandle probeTmpImageHandle = graph.importImage(m_probeTmpImage, "Probe Temp Image", false, {}, m_probeTmpImageState);
	rg::ImageViewHandle probeTmpArrayImageViewHandles[RendererConsts::REFLECTION_PROBE_MIPS] = {};
	rg::ImageViewHandle probeTmpImageViewHandle = 0;
	{
		probeTmpImageViewHandle = graph.createImageView({ "Probe Temp Image", probeTmpImageHandle, { 0, RendererConsts::REFLECTION_PROBE_MIPS, 0, 6 }, ImageViewType::CUBE });
		for (uint32_t i = 0; i < RendererConsts::REFLECTION_PROBE_MIPS; ++i)
		{
			probeTmpArrayImageViewHandles[i] = graph.createImageView({ "Probe Temp Image", probeTmpImageHandle, { i, 1, 0, 6 }, ImageViewType::_2D_ARRAY });
		}
	}

	// relight reflection probes
	for (size_t i = 0; i < data.m_relightCount; ++i)
	{
		const auto &relightData = data.m_lightData->m_reflectionProbeRelightData[data.m_relightProbeIndices[i]];

		// light reflection probes
		LightProbeGBufferPass::Data lightProbeGBufferPassData;
		lightProbeGBufferPassData.m_passRecordContext = data.m_passRecordContext;
		lightProbeGBufferPassData.m_probePosition = relightData.m_position;
		lightProbeGBufferPassData.m_probeNearPlane = relightData.m_nearPlane;
		lightProbeGBufferPassData.m_probeFarPlane = relightData.m_farPlane;
		lightProbeGBufferPassData.m_probeIndex = data.m_relightProbeIndices[i];
		lightProbeGBufferPassData.m_directionalLightsBufferInfo = data.m_directionalLightsBufferInfo;
		lightProbeGBufferPassData.m_directionalLightsShadowedProbeBufferInfo = directionalLightsShadowedProbeBufferInfo;
		lightProbeGBufferPassData.m_depthImageView = m_probeDepthArrayView;
		lightProbeGBufferPassData.m_albedoRoughnessImageView = m_probeAlbedoRoughnessArrayView;
		lightProbeGBufferPassData.m_normalImageView = m_probeNormalArrayView;
		lightProbeGBufferPassData.m_resultImageViewHandle = probeTmpArrayImageViewHandles[0];
		lightProbeGBufferPassData.m_directionalShadowImageViewHandle = m_probeShadowImageViewHandle;
		lightProbeGBufferPassData.m_shadowMatricesBufferInfo = data.m_shadowMatricesBufferInfo;

		LightProbeGBufferPass::addToGraph(graph, lightProbeGBufferPassData);


		// downsample reflection probe
#if 1
		ProbeDownsamplePass::Data probeDownsamplePassData;
		probeDownsamplePassData.m_passRecordContext = data.m_passRecordContext;
		for (size_t j = 0; j < RendererConsts::REFLECTION_PROBE_MIPS; ++j) probeDownsamplePassData.m_resultImageViewHandles[j] = probeTmpArrayImageViewHandles[j];
		for (size_t j = 0; j < RendererConsts::REFLECTION_PROBE_MIPS; ++j) probeDownsamplePassData.m_cubeImageViews[j] = m_probeTmpCubeViews[j];
		
		ProbeDownsamplePass::addToGraph(graph, probeDownsamplePassData);
#else
		ProbeDownsamplePass2::Data probeDownsamplePassData;
		probeDownsamplePassData.m_passRecordContext = data.m_passRecordContext;
		for (size_t j = 0; j < RendererConsts::REFLECTION_PROBE_MIPS; ++j) probeDownsamplePassData.m_resultImageViewHandles[j] = probeTmpArrayImageViewHandles[j];
		
		ProbeDownsamplePass2::addToGraph(graph, probeDownsamplePassData);
#endif


		// filter reflection probe
#if 1
		ProbeFilterPass::Data probeFilterPassData;
		probeFilterPassData.m_passRecordContext = data.m_passRecordContext;
		probeFilterPassData.m_inputImageViewHandle = probeTmpImageViewHandle;
		probeFilterPassData.m_probeFilterCoeffsImageView = m_probeFilterCoeffsImageView;
		for (size_t j = 0; j < RendererConsts::REFLECTION_PROBE_MIPS; ++j) probeFilterPassData.m_resultImageViews[j] = m_probeMipViews[data.m_relightProbeIndices[i]][j];
		
		ProbeFilterPass::addToGraph(graph, probeFilterPassData);
#else
		ProbeFilterImportanceSamplingPass::Data probeFilterPassData;
		probeFilterPassData.m_passRecordContext = data.m_passRecordContext;
		probeFilterPassData.m_inputImageViewHandle = probeTmpImageViewHandle;
		for (size_t j = 0; j < RendererConsts::REFLECTION_PROBE_MIPS; ++j) probeFilterPassData.m_resultImageViews[j] = m_probeMipViews[data.m_relightProbeIndices[i]][j];
		
		ProbeFilterImportanceSamplingPass::addToGraph(graph, probeFilterPassData);
#endif

		// compress lit probe
		//ProbeCompressBCH6Pass::Data compressPassData;
		//compressPassData.m_passRecordContext = data.m_passRecordContext;
		//compressPassData.m_probeIndex = data.m_relightProbeIndices[i];
		//compressPassData.m_compressedCubeArrayImage = m_probeArrayImage;
		//for (size_t j = 0; j < RendererConsts::REFLECTION_PROBE_MIPS; ++j) compressPassData.m_inputImageViews[j] = m_probeUncompressedMipViews[j];
		//for (size_t j = 0; j < RendererConsts::REFLECTION_PROBE_MIPS; ++j) compressPassData.m_tmpResultImageViews[j] = m_probeCompressedTmpMipViews[j];
		//
		//ProbeCompressBCH6Pass::addToGraph(graph, compressPassData);
	}
}

VEngine::gal::ImageView *VEngine::ReflectionProbeModule::getCubeArrayView()
{
	return m_probeCubeArrayView;
}
