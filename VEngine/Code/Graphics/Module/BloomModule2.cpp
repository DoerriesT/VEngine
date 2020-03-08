#include "BloomModule2.h"
#include "Graphics/Pass/BloomDownsamplePass2.h"
#include "Graphics/Pass/BloomUpsamplePass2.h"
#include "Graphics/PassRecordContext2.h"
#include "Graphics/RenderData.h"

using namespace VEngine::gal;

void VEngine::BloomModule2::addToGraph(rg::RenderGraph2 &graph, const InputData &inData, OutputData &outData)
{
	// create images
	rg::ImageHandle downsampleImageHandle;
	rg::ImageHandle upsampleImageHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = inData.m_passRecordContext->m_commonRenderData->m_width / 2;
		desc.m_height = inData.m_passRecordContext->m_commonRenderData->m_height / 2;
		desc.m_layers = 1;
		desc.m_levels = BLOOM_MIP_COUNT;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::B10G11R11_UFLOAT_PACK32;
		desc.m_usageFlags = ImageUsageFlagBits::SAMPLED_BIT;

		desc.m_name = "Bloom Downsample Image";
		downsampleImageHandle = graph.createImage(desc);

		desc.m_name = "Bloom Upsample Image";
		upsampleImageHandle = graph.createImage(desc);
	}

	// create views
	rg::ImageViewHandle downsampleViewHandles[BLOOM_MIP_COUNT];
	for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i)
	{
		rg::ImageViewDescription viewDesc{};
		viewDesc.m_name = "Bloom Downsample Mip Level";
		viewDesc.m_imageHandle = downsampleImageHandle;
		viewDesc.m_subresourceRange = { i, 1, 0, 1 };

		downsampleViewHandles[i] = graph.createImageView(viewDesc);
	}

	rg::ImageViewHandle upsampleViewHandles[BLOOM_MIP_COUNT];
	for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i)
	{
		rg::ImageViewDescription viewDesc{};
		viewDesc.m_name = "Bloom Upsample Mip Level";
		viewDesc.m_imageHandle = upsampleImageHandle;
		viewDesc.m_subresourceRange = { i, 1, 0, 1 };

		upsampleViewHandles[i] = graph.createImageView(viewDesc);
	}

	// downsample
	BloomDownsamplePass2::Data downsamplePassData;
	downsamplePassData.m_passRecordContext = inData.m_passRecordContext;
	downsamplePassData.m_inputImageViewHandle = inData.m_colorImageViewHandle;
	for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i)
	{
		downsamplePassData.m_resultImageViewHandles[i] = downsampleViewHandles[i];
	}

	BloomDownsamplePass2::addToGraph(graph, downsamplePassData);


	// upsample
	BloomUpsamplePass2::Data upsamplePassData;
	upsamplePassData.m_passRecordContext = inData.m_passRecordContext;
	for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i)
	{
		upsamplePassData.m_inputImageViewHandles[i] = downsampleViewHandles[i];
		upsamplePassData.m_resultImageViewHandles[i] = upsampleViewHandles[i];
	}

	BloomUpsamplePass2::addToGraph(graph, upsamplePassData);


	outData.m_bloomImageViewHandle = upsampleViewHandles[0];
}
