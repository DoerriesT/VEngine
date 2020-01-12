#include "BloomModule.h"
#include "Graphics/Vulkan/Pass/BloomDownsamplePass.h"
#include "Graphics/Vulkan/Pass/BloomUpsamplePass.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

void VEngine::BloomModule::addToGraph(RenderGraph &graph, const InputData &inData, OutputData &outData)
{
	// create images
	ImageHandle downsampleImageHandle;
	ImageHandle upsampleImageHandle;
	{
		ImageDescription desc = {};
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = inData.m_passRecordContext->m_commonRenderData->m_width / 2;
		desc.m_height = inData.m_passRecordContext->m_commonRenderData->m_height / 2;
		desc.m_layers = 1;
		desc.m_levels = BLOOM_MIP_COUNT;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		desc.m_usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;

		desc.m_name = "Bloom Downsample Image";
		downsampleImageHandle = graph.createImage(desc);

		desc.m_name = "Bloom Upsample Image";
		upsampleImageHandle = graph.createImage(desc);
	}

	// create views
	ImageViewHandle downsampleViewHandles[BLOOM_MIP_COUNT];
	for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i)
	{
		ImageViewDescription viewDesc{};
		viewDesc.m_name = "Bloom Downsample Mip Level";
		viewDesc.m_imageHandle = downsampleImageHandle;
		viewDesc.m_subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 };

		downsampleViewHandles[i] = graph.createImageView(viewDesc);
	}

	ImageViewHandle upsampleViewHandles[BLOOM_MIP_COUNT];
	for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i)
	{
		ImageViewDescription viewDesc{};
		viewDesc.m_name = "Bloom Upsample Mip Level";
		viewDesc.m_imageHandle = upsampleImageHandle;
		viewDesc.m_subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 };

		upsampleViewHandles[i] = graph.createImageView(viewDesc);
	}

	// downsample
	BloomDownsamplePass::Data downsamplePassData;
	downsamplePassData.m_passRecordContext = inData.m_passRecordContext;
	downsamplePassData.m_inputImageViewHandle = inData.m_colorImageViewHandle;
	for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i)
	{
		downsamplePassData.m_resultImageViewHandles[i] = downsampleViewHandles[i];
	}

	BloomDownsamplePass::addToGraph(graph, downsamplePassData);


	// upsample
	BloomUpsamplePass::Data upsamplePassData;
	upsamplePassData.m_passRecordContext = inData.m_passRecordContext;
	for (uint32_t i = 0; i < BLOOM_MIP_COUNT; ++i)
	{
		upsamplePassData.m_inputImageViewHandles[i] = downsampleViewHandles[i];
		upsamplePassData.m_resultImageViewHandles[i] = upsampleViewHandles[i];
	}

	BloomUpsamplePass::addToGraph(graph, upsamplePassData);


	outData.m_bloomImageViewHandle = upsampleViewHandles[0];
}
