#include "SSRModule.h"
#include "Utility/Utility.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/Pass/SSRPass.h"
#include "Graphics/Pass/SSRResolvePass.h"
#include "Graphics/Pass/SSRTemporalFilterPass.h"

using namespace VEngine::gal;

extern float g_ssrBias;

VEngine::SSRModule::SSRModule(gal::GraphicsDevice *graphicsDevice, uint32_t width, uint32_t height)
	:m_graphicsDevice(graphicsDevice),
	m_width(width),
	m_height(height)
{
	resize(width, height);
}

VEngine::SSRModule::~SSRModule()
{
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_graphicsDevice->destroyImage((m_ssrHistoryImages[i]));
	}
}

void VEngine::SSRModule::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	auto &commonData = *data.m_passRecordContext->m_commonRenderData;

	rg::ImageViewHandle ssrPreviousImageViewHandle = 0;
	{
		rg::ImageHandle imageHandle = graph.importImage(m_ssrHistoryImages[commonData.m_prevResIdx], "SSR Previous Image", false, {}, &m_ssrHistoryImageState[commonData.m_prevResIdx]);
		ssrPreviousImageViewHandle = graph.createImageView({ "SSR Previous Image", imageHandle, { 0, 1, 0, 1 } });
	}

	// current ssr image view handle
	{
		rg::ImageHandle imageHandle = graph.importImage(m_ssrHistoryImages[commonData.m_curResIdx], "SSR Image", false, {}, &m_ssrHistoryImageState[commonData.m_curResIdx]);
		m_ssrImageViewHandle = graph.createImageView({ "SSR Image", imageHandle, { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle ssrRayHitPdfImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "SSR RayHit/PDF Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width / 2;
		desc.m_height = m_height / 2;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R16G16B16A16_SFLOAT;

		ssrRayHitPdfImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle ssrMaskImageViewHandle;
	rg::ImageViewHandle resolvedMaskImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "SSR Mask Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width / 2;
		desc.m_height = m_height / 2;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R8_UNORM;

		ssrMaskImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });

		desc.m_name = "SSR Resolved Mask Image";
		desc.m_width = m_width;
		desc.m_height = m_height;
		resolvedMaskImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle resolvedColorRayDepthImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "SSR Resolved Color/Ray Depth Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R16G16B16A16_SFLOAT;

		resolvedColorRayDepthImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}


	// trace rays
	SSRPass::Data ssrPassData;
	ssrPassData.m_passRecordContext = data.m_passRecordContext;
	ssrPassData.m_bias = g_ssrBias;
	ssrPassData.m_noiseTextureHandle = data.m_noiseTextureHandle;
	ssrPassData.m_hiZPyramidImageHandle = data.m_hiZPyramidImageViewHandle;
	ssrPassData.m_normalImageHandle = data.m_normalImageViewHandle;
	ssrPassData.m_specularRoughnessImageHandle = data.m_specularRoughnessImageViewHandle;
	ssrPassData.m_rayHitPDFImageHandle = ssrRayHitPdfImageViewHandle;
	ssrPassData.m_maskImageHandle = ssrMaskImageViewHandle;

	SSRPass::addToGraph(graph, ssrPassData);


	// resolve color
	SSRResolvePass::Data ssrResolvePassData;
	ssrResolvePassData.m_passRecordContext = data.m_passRecordContext;
	ssrResolvePassData.m_ignoreHistory = data.m_ignoreHistory;
	ssrResolvePassData.m_bias = g_ssrBias;
	ssrResolvePassData.m_noiseTextureHandle = data.m_noiseTextureHandle;
	ssrResolvePassData.m_exposureDataBufferHandle = data.m_exposureDataBufferHandle;
	ssrResolvePassData.m_rayHitPDFImageHandle = ssrRayHitPdfImageViewHandle;
	ssrResolvePassData.m_maskImageHandle = ssrMaskImageViewHandle;
	ssrResolvePassData.m_depthImageHandle = data.m_depthImageViewHandle;
	ssrResolvePassData.m_normalImageHandle = data.m_normalImageViewHandle;
	ssrResolvePassData.m_specularRoughnessImageHandle = data.m_specularRoughnessImageViewHandle;
	ssrResolvePassData.m_prevColorImageHandle = data.m_prevColorImageViewHandle;
	ssrResolvePassData.m_velocityImageHandle = data.m_velocityImageViewHandle;
	ssrResolvePassData.m_resultImageHandle = resolvedColorRayDepthImageViewHandle;
	ssrResolvePassData.m_resultMaskImageHandle = resolvedMaskImageViewHandle;

	SSRResolvePass::addToGraph(graph, ssrResolvePassData);

	//m_ssrImageViewHandle = resolvedColorRayDepthImageViewHandle;


	// temporal filter
	SSRTemporalFilterPass::Data ssrTemporalFilterPassData;
	ssrTemporalFilterPassData.m_passRecordContext = data.m_passRecordContext;
	ssrTemporalFilterPassData.m_ignoreHistory = data.m_ignoreHistory;
	ssrTemporalFilterPassData.m_exposureDataBufferHandle = data.m_exposureDataBufferHandle;
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
		if (m_ssrHistoryImages[i])
		{
			m_graphicsDevice->destroyImage(m_ssrHistoryImages[i]);
		}
	}

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
		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_ssrHistoryImages[i]);
		m_ssrHistoryImageState[i] = {};
	}
}

VEngine::rg::ImageViewHandle VEngine::SSRModule::getSSRResultImageViewHandle()
{
	return m_ssrImageViewHandle;
}
