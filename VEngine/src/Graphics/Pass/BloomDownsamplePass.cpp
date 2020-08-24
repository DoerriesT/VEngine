#include "BloomDownsamplePass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/bloomDownsample.hlsli"
}

void VEngine::BloomDownsamplePass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[1 + BloomModule::BLOOM_MIP_COUNT];

	passUsages[0] = { rg::ResourceViewHandle(data.m_inputImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
	for (uint32_t i = 0; i < BloomModule::BLOOM_MIP_COUNT; ++i)
	{
		// last level isnt read from in this pass
		if (i == (BloomModule::BLOOM_MIP_COUNT - 1))
		{
			passUsages[i + 1] = { rg::ResourceViewHandle(data.m_resultImageViewHandles[i]), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }};
		}
		else
		{
			passUsages[i + 1] = { rg::ResourceViewHandle(data.m_resultImageViewHandles[i]), {gal::ResourceState::WRITE_RW_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }, true, {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
		}
	}

	graph.addPass("Bloom Downsample", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/bloomDownsample_cs");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			uint32_t mipWidth = glm::max(data.m_passRecordContext->m_commonRenderData->m_width / 2u, 1u);
			uint32_t mipHeight = glm::max(data.m_passRecordContext->m_commonRenderData->m_height / 2u, 1u);

			for (uint32_t i = 0; i < BloomModule::BLOOM_MIP_COUNT; ++i)
			{
				// update descriptor set
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));
					
					ImageView *inputImageView = registry.getImageView(i == 0 ? data.m_inputImageViewHandle : data.m_resultImageViewHandles[i - 1]);
					ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandles[i]);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
						Initializers::texture(&inputImageView, INPUT_IMAGE_BINDING),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);
					
					DescriptorSet *sets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
				}

				PushConsts pushConsts;
				pushConsts.texelSize = 1.0f / glm::vec2(mipWidth, mipHeight);
				pushConsts.width = mipWidth;
				pushConsts.height = mipHeight;
				pushConsts.doWeightedAverage = i == 0;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				cmdList->dispatch((mipWidth + 15) / 16, (mipHeight + 15) / 16, 1);

				mipWidth = glm::max(mipWidth / 2u, 1u);
				mipHeight = glm::max(mipHeight / 2u, 1u);
				

				// dont insert a barrier after the last iteration: we dont know the next resource state, so let the RenderGraph figure it out
				if (i < (BloomModule::BLOOM_MIP_COUNT - 1))
				{
					Barrier barrier = Initializers::imageBarrier(registry.getImage(data.m_resultImageViewHandles[i]), 
						PipelineStageFlagBits::COMPUTE_SHADER_BIT, 
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						gal::ResourceState::WRITE_RW_IMAGE,
						gal::ResourceState::READ_TEXTURE,
						{i, 1, 0, 1});
					
					cmdList->barrier(1, &barrier);
				}
			}
		}, true);
}
