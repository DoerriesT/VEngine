#include "HiZPyramidPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/hiZPyramid.hlsli"
}

void VEngine::HiZPyramidPass::addToGraph(rg::RenderGraph &graph, const Data &data, OutData &outData)
{
	uint32_t levels = 1;
	{
		uint32_t w = data.m_passRecordContext->m_commonRenderData->m_width;
		uint32_t h = data.m_passRecordContext->m_commonRenderData->m_height;
		while (w > 1 || h > 1)
		{
			++levels;
			w /= 2;
			h /= 2;
		}
	}

	uint32_t resultWidth = data.m_passRecordContext->m_commonRenderData->m_width;
	uint32_t resultHeight = data.m_passRecordContext->m_commonRenderData->m_height;

	if (!data.m_copyFirstLevel)
	{
		resultWidth = glm::max(resultWidth / 2u, 1u);
		resultHeight = glm::max(resultHeight / 2u, 1u);
		levels = glm::max(levels - 1u, 1u);
	}

	// create result image
	rg::ImageDescription desc = {};
	desc.m_name = "Hi-Z Pyramid Image";
	desc.m_clear = false;
	desc.m_clearValue.m_imageClearValue = {};
	desc.m_width = resultWidth;
	desc.m_height = resultHeight;
	desc.m_layers = 1;
	desc.m_levels = levels;
	desc.m_samples = SampleCount::_1;
	desc.m_format = Format::R32_SFLOAT;
	desc.m_usageFlags = ImageUsageFlagBits::TEXTURE_BIT | ImageUsageFlagBits::TRANSFER_DST_BIT | ImageUsageFlagBits::RW_TEXTURE_BIT;

	outData.m_resultImageHandle = graph.createImage(desc);

	rg::ImageViewDescription viewDesc{};
	viewDesc.m_name = "Hi-Z Pyramid";
	viewDesc.m_imageHandle = outData.m_resultImageHandle;
	viewDesc.m_subresourceRange = { 0, levels, 0, 1 };
	outData.m_resultImageViewHandle = graph.createImageView(viewDesc);

	// create image views for each mip
	rg::ImageViewHandle viewHandles[14];
	for (uint32_t i = 0; i < levels; ++i)
	{
		rg::ImageViewDescription viewDesc{};
		viewDesc.m_name = "Hi-Z Pyramid Mip Level";
		viewDesc.m_imageHandle = outData.m_resultImageHandle;
		viewDesc.m_subresourceRange = { i, 1, 0, 1 };
		viewHandles[i] = graph.createImageView(viewDesc);
	}

	rg::ResourceUsageDescription passUsages[15]; // 14 levels maximum + input image
	for (uint32_t i = 0; i < levels; ++i)
	{
		rg::ResourceViewHandle resViewHandle = rg::ResourceViewHandle(viewHandles[i]);

		// last level isnt read from in this pass
		if (i == (levels - 1))
		{
			passUsages[i] = { resViewHandle, {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} };
		}
		else
		{
			passUsages[i] = { resViewHandle, {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}, true, {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} };
		}
	}
	passUsages[levels] = { rg::ResourceViewHandle(data.m_inputImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} };

	graph.addPass("Hi-Z Pyramid", rg::QueueType::GRAPHICS, levels + 1, passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader(data.m_maxReduction ? "Resources/Shaders/hlsl/hiZPyramid_MAX_cs" : "Resources/Shaders/hlsl/hiZPyramid_MIN_cs");
			
			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			uint32_t mipWidth = data.m_passRecordContext->m_commonRenderData->m_width;
			uint32_t mipHeight = data.m_passRecordContext->m_commonRenderData->m_height;

			for (uint32_t i = 0; i < levels; ++i)
			{
				// update descriptor set
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *inputImageView = registry.getImageView(i == 0 ? data.m_inputImageViewHandle : viewHandles[i - 1]);
					ImageView *resultImageView = registry.getImageView(viewHandles[i]);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::texture(&inputImageView, INPUT_IMAGE_BINDING),
						Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);

					DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
				}

				PushConsts pushConsts;
				pushConsts.prevMipSize = glm::ivec2(mipWidth, mipHeight);
				if (i != 0 || !data.m_copyFirstLevel)
				{
					mipWidth = glm::max(mipWidth / 2u, 1u);
					mipHeight = glm::max(mipHeight / 2u, 1u);
				}
				pushConsts.width = mipWidth;
				pushConsts.height = mipHeight;
				pushConsts.copyOnly = (i == 0 && data.m_copyFirstLevel) ? 1 : 0;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				cmdList->dispatch((mipWidth + 15) / 16, (mipHeight + 15) / 16, 1);

				// dont insert a barrier after the last iteration: we dont know the next state, so let the RenderGraph figure it out
				if (i < (levels - 1))
				{
					Barrier barrier = Initializers::imageBarrier(registry.getImage(viewHandles[i]),
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						gal::ResourceState::WRITE_RW_IMAGE,
						gal::ResourceState::READ_TEXTURE,
						{ i, 1, 0, 1 });

					cmdList->barrier(1, &barrier);
				}
			}
		}, data.m_forceExecution);
}
