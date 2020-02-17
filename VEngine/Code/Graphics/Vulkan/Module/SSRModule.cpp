#include "SSRModule.h"
#include "Utility/Utility.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/Pass/SSRPass.h"
#include "Graphics/Vulkan/Pass/SSRResolvePass.h"
#include "Graphics/Vulkan/Pass/SSRTemporalFilterPass.h"

extern float g_ssrBias;

VEngine::SSRModule::SSRModule(uint32_t width, uint32_t height) :m_width(width),
m_height(height)
{
	resize(width, height);
}

VEngine::SSRModule::~SSRModule()
{
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		g_context.m_allocator.destroyImage(m_ssrHistoryImages[i], m_ssrHistoryImageAllocHandles[i]);
	}
}

void VEngine::SSRModule::addToGraph(RenderGraph &graph, const Data &data)
{
	auto &commonData = *data.m_passRecordContext->m_commonRenderData;

	ImageViewHandle ssrPreviousImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "SSR Previous Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		ImageHandle imageHandle = graph.importImage(desc, m_ssrHistoryImages[commonData.m_prevResIdx], &m_ssrHistoryImageQueue[commonData.m_prevResIdx], &m_ssrHistoryImageResourceState[commonData.m_prevResIdx]);
		ssrPreviousImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	// current ssr image view handle
	{
		ImageDescription desc = {};
		desc.m_name = "SSR Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		ImageHandle imageHandle = graph.importImage(desc, m_ssrHistoryImages[commonData.m_curResIdx], &m_ssrHistoryImageQueue[commonData.m_curResIdx], &m_ssrHistoryImageResourceState[commonData.m_curResIdx]);
		m_ssrImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle ssrRayHitPdfImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_name = "SSR RayHit/PDF Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		ssrRayHitPdfImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle ssrMaskImageViewHandle;
	ImageViewHandle resolvedMaskImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_name = "SSR Mask Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R8_UNORM;

		ssrMaskImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });

		desc.m_name = "SSR Resolved Mask Image";
		resolvedMaskImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle resolvedColorRayDepthImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_name = "SSR Resolved Color/Ray Depth Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		resolvedColorRayDepthImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}


	// trace rays
	SSRPass::Data ssrPassData;
	ssrPassData.m_passRecordContext = data.m_passRecordContext;
	ssrPassData.m_bias = g_ssrBias;
	ssrPassData.m_noiseTextureHandle = data.m_noiseTextureHandle;
	ssrPassData.m_hiZPyramidImageHandle = data.m_hiZPyramidImageViewHandle;
	ssrPassData.m_normalImageHandle = data.m_normalImageViewHandle;
	ssrPassData.m_rayHitPDFImageHandle = ssrRayHitPdfImageViewHandle;
	ssrPassData.m_maskImageHandle = ssrMaskImageViewHandle;

	SSRPass::addToGraph(graph, ssrPassData);


	// resolve color
	SSRResolvePass::Data ssrResolvePassData;
	ssrResolvePassData.m_passRecordContext = data.m_passRecordContext;
	ssrResolvePassData.m_bias = g_ssrBias;
	ssrResolvePassData.m_noiseTextureHandle = data.m_noiseTextureHandle;
	ssrResolvePassData.m_rayHitPDFImageHandle = ssrRayHitPdfImageViewHandle;
	ssrResolvePassData.m_maskImageHandle = ssrMaskImageViewHandle;
	ssrResolvePassData.m_depthImageHandle = data.m_depthImageViewHandle;
	ssrResolvePassData.m_normalImageHandle = data.m_normalImageViewHandle;
	ssrResolvePassData.m_albedoImageHandle = data.m_albedoImageViewHandle;
	ssrResolvePassData.m_prevColorImageHandle = data.m_prevColorImageViewHandle;
	ssrResolvePassData.m_velocityImageHandle = data.m_velocityImageViewHandle;
	ssrResolvePassData.m_resultImageHandle = resolvedColorRayDepthImageViewHandle;
	ssrResolvePassData.m_resultMaskImageHandle = resolvedMaskImageViewHandle;

	SSRResolvePass::addToGraph(graph, ssrResolvePassData);

	//m_ssrImageViewHandle = resolvedColorRayDepthImageViewHandle;


	// temporal filter
	SSRTemporalFilterPass::Data ssrTemporalFilterPassData;
	ssrTemporalFilterPassData.m_passRecordContext = data.m_passRecordContext;
	ssrTemporalFilterPassData.m_resultImageViewHandle = m_ssrImageViewHandle;
	ssrTemporalFilterPassData.m_historyImageViewHandle = ssrPreviousImageViewHandle;
	ssrTemporalFilterPassData.m_colorRayDepthImageViewHandle = resolvedColorRayDepthImageViewHandle;
	ssrTemporalFilterPassData.m_maskImageViewHandle = resolvedMaskImageViewHandle;

	SSRTemporalFilterPass::addToGraph(graph, ssrTemporalFilterPassData);
}

void VEngine::SSRModule::resize(uint32_t width, uint32_t height)
{
	m_width = width;
	m_height = height;

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		if (m_ssrHistoryImageAllocHandles[i] != VK_NULL_HANDLE)
		{
			g_context.m_allocator.destroyImage(m_ssrHistoryImages[i], m_ssrHistoryImageAllocHandles[i]);
		}
	}

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
		if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_ssrHistoryImages[i], m_ssrHistoryImageAllocHandles[i]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image!", EXIT_FAILURE);
		}
		m_ssrHistoryImageQueue[i] = RenderGraph::undefinedQueue;
		m_ssrHistoryImageResourceState[i] = ResourceState::UNDEFINED;
	}
}

VEngine::ImageViewHandle VEngine::SSRModule::getSSRResultImageViewHandle()
{
	return m_ssrImageViewHandle;
}
