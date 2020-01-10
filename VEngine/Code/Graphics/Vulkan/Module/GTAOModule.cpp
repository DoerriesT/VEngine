#include "GTAOModule.h"
#include "Graphics/Vulkan/Pass/VKGTAOPass.h"
#include "Graphics/Vulkan/Pass/VKGTAOSpatialFilterPass.h"
#include "Graphics/Vulkan/Pass/VKGTAOTemporalFilterPass.h"
#include "Utility/Utility.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

VEngine::GTAOModule::GTAOModule(uint32_t width, uint32_t height)
	:m_width(width),
	m_height(height)
{
	resize(width, height);
}


VEngine::GTAOModule::~GTAOModule()
{
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		g_context.m_allocator.destroyImage(m_gtaoHistoryTextures[i], m_gtaoHistoryTextureAllocHandles[i]);
	}
}

void VEngine::GTAOModule::addToGraph(RenderGraph &graph, const Data &data)
{
	auto &commonData = *data.m_passRecordContext->m_commonRenderData;

	// result image
	{
		ImageDescription desc = {};
		desc.m_name = "GTAO Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = VK_FORMAT_R16G16_SFLOAT;

		ImageHandle gtaoImageHandle = graph.importImage(desc, m_gtaoHistoryTextures[commonData.m_curResIdx], &m_gtaoHistoryTextureQueue[commonData.m_curResIdx], &m_gtaoHistoryTextureResourceState[commonData.m_curResIdx]);
		m_gtaoImageViewHandle = graph.createImageView({ desc.m_name, gtaoImageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle gtaoPreviousImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "GTAO Previous Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = VK_FORMAT_R16G16_SFLOAT;

		ImageHandle gtaoPreviousImageHandle = graph.importImage(desc, m_gtaoHistoryTextures[commonData.m_prevResIdx], &m_gtaoHistoryTextureQueue[commonData.m_prevResIdx], &m_gtaoHistoryTextureResourceState[commonData.m_prevResIdx]);
		gtaoPreviousImageViewHandle = graph.createImageView({ desc.m_name, gtaoPreviousImageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle gtaoRawImageViewHandle;
	ImageViewHandle gtaoSpatiallyFilteredImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_name = "GTAO Temp Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16_SFLOAT;

		gtaoRawImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
		gtaoSpatiallyFilteredImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}


	// gtao
	VKGTAOPass::Data gtaoPassData;
	gtaoPassData.m_passRecordContext = data.m_passRecordContext;
	gtaoPassData.m_depthImageHandle = data.m_depthImageViewHandle;
	gtaoPassData.m_tangentSpaceImageHandle = data.m_tangentSpaceImageViewHandle;
	gtaoPassData.m_resultImageHandle = gtaoRawImageViewHandle;

	VKGTAOPass::addToGraph(graph, gtaoPassData);


	// gtao spatial filter
	VKGTAOSpatialFilterPass::Data gtaoPassSpatialFilterPassData;
	gtaoPassSpatialFilterPassData.m_passRecordContext = data.m_passRecordContext;
	gtaoPassSpatialFilterPassData.m_inputImageHandle = gtaoRawImageViewHandle;
	gtaoPassSpatialFilterPassData.m_resultImageHandle = gtaoSpatiallyFilteredImageViewHandle;

	VKGTAOSpatialFilterPass::addToGraph(graph, gtaoPassSpatialFilterPassData);


	// gtao temporal filter
	VKGTAOTemporalFilterPass::Data gtaoPassTemporalFilterPassData;
	gtaoPassTemporalFilterPassData.m_passRecordContext = data.m_passRecordContext;
	gtaoPassTemporalFilterPassData.m_inputImageHandle = gtaoSpatiallyFilteredImageViewHandle;
	gtaoPassTemporalFilterPassData.m_velocityImageHandle = data.m_velocityImageViewHandle;
	gtaoPassTemporalFilterPassData.m_previousImageHandle = gtaoPreviousImageViewHandle;
	gtaoPassTemporalFilterPassData.m_resultImageHandle = m_gtaoImageViewHandle;

	VKGTAOTemporalFilterPass::addToGraph(graph, gtaoPassTemporalFilterPassData);
}

void VEngine::GTAOModule::resize(uint32_t width, uint32_t height)
{
	m_width = width;
	m_height = height;

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		if (m_gtaoHistoryTextureAllocHandles[i] != VK_NULL_HANDLE)
		{
			g_context.m_allocator.destroyImage(m_gtaoHistoryTextures[i], m_gtaoHistoryTextureAllocHandles[i]);
		}
	}

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
		if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_gtaoHistoryTextures[i], m_gtaoHistoryTextureAllocHandles[i]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image!", EXIT_FAILURE);
		}
		m_gtaoHistoryTextureQueue[i] = RenderGraph::undefinedQueue;
		m_gtaoHistoryTextureResourceState[i] = ResourceState::UNDEFINED;
	}
}

VEngine::ImageViewHandle VEngine::GTAOModule::getAOResultImageViewHandle()
{
	return m_gtaoImageViewHandle;
}
