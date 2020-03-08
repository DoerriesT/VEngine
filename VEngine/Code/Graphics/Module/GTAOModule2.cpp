#include "GTAOModule2.h"
#include "Graphics/Pass/GTAOPass2.h"
#include "Graphics/Pass/GTAOSpatialFilterPass2.h"
#include "Graphics/Pass/GTAOTemporalFilterPass2.h"
#include "Utility/Utility.h"
#include "Graphics/PassRecordContext2.h"
#include "Graphics/RenderData.h"

using namespace VEngine::gal;

VEngine::GTAOModule2::GTAOModule2(gal::GraphicsDevice *graphicsDevice, uint32_t width, uint32_t height)
	:m_graphicsDevice(graphicsDevice),
	m_width(width),
	m_height(height)
{
	resize(width, height);
}


VEngine::GTAOModule2::~GTAOModule2()
{
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_graphicsDevice->destroyImage(m_gtaoHistoryTextures[i]);
	}
}

void VEngine::GTAOModule2::addToGraph(rg::RenderGraph2 &graph, const Data &data)
{
	auto &commonData = *data.m_passRecordContext->m_commonRenderData;

	// result image
	{
		rg::ImageHandle gtaoImageHandle = graph.importImage(m_gtaoHistoryTextures[commonData.m_curResIdx], "GTAO Image", false, {}, &m_gtaoHistoryTextureState[commonData.m_curResIdx]);
		m_gtaoImageViewHandle = graph.createImageView({ "GTAO Image", gtaoImageHandle, { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle gtaoPreviousImageViewHandle = 0;
	{
		rg::ImageHandle gtaoPreviousImageHandle = graph.importImage(m_gtaoHistoryTextures[commonData.m_prevResIdx], "GTAO Previous Image", false, {}, &m_gtaoHistoryTextureState[commonData.m_prevResIdx]);
		gtaoPreviousImageViewHandle = graph.createImageView({ "GTAO Previous Image", gtaoPreviousImageHandle, { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle gtaoRawImageViewHandle;
	rg::ImageViewHandle gtaoSpatiallyFilteredImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "GTAO Temp Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R16G16_SFLOAT;

		gtaoRawImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
		gtaoSpatiallyFilteredImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}


	// gtao
	GTAOPass2::Data gtaoPassData;
	gtaoPassData.m_passRecordContext = data.m_passRecordContext;
	gtaoPassData.m_depthImageHandle = data.m_depthImageViewHandle;
	gtaoPassData.m_tangentSpaceImageHandle = data.m_tangentSpaceImageViewHandle;
	gtaoPassData.m_resultImageHandle = gtaoRawImageViewHandle;

	GTAOPass2::addToGraph(graph, gtaoPassData);


	// gtao spatial filter
	GTAOSpatialFilterPass2::Data gtaoPassSpatialFilterPassData;
	gtaoPassSpatialFilterPassData.m_passRecordContext = data.m_passRecordContext;
	gtaoPassSpatialFilterPassData.m_inputImageHandle = gtaoRawImageViewHandle;
	gtaoPassSpatialFilterPassData.m_resultImageHandle = gtaoSpatiallyFilteredImageViewHandle;

	GTAOSpatialFilterPass2::addToGraph(graph, gtaoPassSpatialFilterPassData);


	// gtao temporal filter
	GTAOTemporalFilterPass2::Data gtaoPassTemporalFilterPassData;
	gtaoPassTemporalFilterPassData.m_passRecordContext = data.m_passRecordContext;
	gtaoPassTemporalFilterPassData.m_inputImageHandle = gtaoSpatiallyFilteredImageViewHandle;
	gtaoPassTemporalFilterPassData.m_velocityImageHandle = data.m_velocityImageViewHandle;
	gtaoPassTemporalFilterPassData.m_previousImageHandle = gtaoPreviousImageViewHandle;
	gtaoPassTemporalFilterPassData.m_resultImageHandle = m_gtaoImageViewHandle;

	GTAOTemporalFilterPass2::addToGraph(graph, gtaoPassTemporalFilterPassData);
}

void VEngine::GTAOModule2::resize(uint32_t width, uint32_t height)
{
	m_width = width;
	m_height = height;

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		if (m_gtaoHistoryTextures[i])
		{
			m_graphicsDevice->destroyImage(m_gtaoHistoryTextures[i]);
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
	imageCreateInfo.m_format = Format::R16G16_SFLOAT;
	imageCreateInfo.m_createFlags = 0;
	imageCreateInfo.m_usageFlags = ImageUsageFlagBits::STORAGE_BIT | ImageUsageFlagBits::SAMPLED_BIT;


	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_gtaoHistoryTextures[i]);

		m_gtaoHistoryTextureState[i] = {};
	}
}

VEngine::rg::ImageViewHandle VEngine::GTAOModule2::getAOResultImageViewHandle()
{
	return m_gtaoImageViewHandle;
}
