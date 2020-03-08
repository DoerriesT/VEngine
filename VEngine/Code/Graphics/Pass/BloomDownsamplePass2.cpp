#include "BloomDownsamplePass2.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext2.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	using namespace glm;
#include "../../../../Application/Resources/Shaders/bloomDownsample_bindings.h"
}

void VEngine::BloomDownsamplePass2::addToGraph(rg::RenderGraph2 &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[1 + BloomModule::BLOOM_MIP_COUNT];

	passUsages[0] = { rg::ResourceViewHandle(data.m_inputImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
	for (uint32_t i = 0; i < BloomModule::BLOOM_MIP_COUNT; ++i)
	{
		// last level isnt read from in this pass
		if (i == (BloomModule::BLOOM_MIP_COUNT - 1))
		{
			passUsages[i + 1] = { rg::ResourceViewHandle(data.m_resultImageViewHandles[i]), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }};
		}
		else
		{
			passUsages[i + 1] = { rg::ResourceViewHandle(data.m_resultImageViewHandles[i]), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }, true, {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
		}
	}

	graph.addPass("Bloom Downsample", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry2 &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/bloomDownsample_comp.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			Sampler *linearSampler = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX];
			uint32_t mipWidth = glm::max(data.m_passRecordContext->m_commonRenderData->m_width / 2u, 1u);
			uint32_t mipHeight = glm::max(data.m_passRecordContext->m_commonRenderData->m_height / 2u, 1u);

			for (uint32_t i = 0; i < BloomModule::BLOOM_MIP_COUNT; ++i)
			{
				// update descriptor set
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));
					
					ImageView *inputImageView = registry.getImageView(i == 0 ? data.m_inputImageViewHandle : data.m_resultImageViewHandles[i - 1]);
					ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandles[i]);

					DescriptorSetUpdate updates[] =
					{
						Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
						Initializers::sampledImage(&inputImageView, INPUT_IMAGE_BINDING),
						Initializers::samplerDescriptor(&linearSampler, SAMPLER_BINDING),
					};

					descriptorSet->update(2, updates);
					
					cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
				}

				PushConsts pushConsts;
				pushConsts.texelSize = 1.0f / glm::vec2(mipWidth, mipHeight);
				pushConsts.width = mipWidth;
				pushConsts.height = mipHeight;
				pushConsts.doWeightedAverage = i == 0;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				cmdList->dispatch((mipWidth + 15) / 16, (mipHeight + 15) / 16, 1);

				mipWidth = max(mipWidth / 2u, 1u);
				mipHeight = max(mipHeight / 2u, 1u);
				

				// dont insert a barrier after the last iteration: we dont know the next resource state, so let the RenderGraph figure it out
				if (i < (BloomModule::BLOOM_MIP_COUNT - 1))
				{
					Barrier barrier = Initializers::imageBarrier(registry.getImage(data.m_resultImageViewHandles[i]), 
						PipelineStageFlagBits::COMPUTE_SHADER_BIT, 
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						gal::ResourceState::WRITE_STORAGE_IMAGE,
						gal::ResourceState::READ_TEXTURE,
						{i, 1, 0, 1});
					
					cmdList->barrier(1, &barrier);
				}
			}
		}, true);
}
