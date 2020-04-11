#include "BloomUpsamplePass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/bloomUpsample.hlsli"
}

void VEngine::BloomUpsamplePass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[BloomModule::BLOOM_MIP_COUNT * 2];
	for (uint32_t i = 0; i < BloomModule::BLOOM_MIP_COUNT; ++i)
	{
		passUsages[i] = { rg::ResourceViewHandle(data.m_inputImageViewHandles[i]), { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
	}
	for (uint32_t i = 0; i < BloomModule::BLOOM_MIP_COUNT; ++i)
	{
		// first level isnt read from in this pass
		if (i == 0)
		{
			passUsages[i + BloomModule::BLOOM_MIP_COUNT] = { rg::ResourceViewHandle(data.m_resultImageViewHandles[i]), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
		}
		else
		{
			passUsages[i + BloomModule::BLOOM_MIP_COUNT] = { rg::ResourceViewHandle(data.m_resultImageViewHandles[i]), { gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }, true, { gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
		}
	}


	graph.addPass("Bloom Upsample", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/bloomUpsample_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			Sampler *linearSampler = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX];

			for (uint32_t i = 0; i < BloomModule::BLOOM_MIP_COUNT - 1; ++i)
			{
				uint32_t curViewIdx = BloomModule::BLOOM_MIP_COUNT - i - 2;

				// update descriptor set
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *inputImageView = registry.getImageView(i == 0 ? data.m_inputImageViewHandles[curViewIdx + 1] : data.m_resultImageViewHandles[curViewIdx + 1]);
					ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandles[curViewIdx]);
					ImageView *prevResultImageView = registry.getImageView(data.m_inputImageViewHandles[curViewIdx]);

					DescriptorSetUpdate updates[] =
					{
						Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
						Initializers::sampledImage(&inputImageView, INPUT_IMAGE_BINDING),
						Initializers::sampledImage(&prevResultImageView, PREV_RESULT_IMAGE_BINDING),
						Initializers::samplerDescriptor(&linearSampler, SAMPLER_BINDING),
					};

					descriptorSet->update(4, updates);

					cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
				}

				uint32_t mipWidth = glm::max(data.m_passRecordContext->m_commonRenderData->m_width / 2u / (1 << curViewIdx), 1u);
				uint32_t mipHeight = glm::max(data.m_passRecordContext->m_commonRenderData->m_height / 2u / (1 << curViewIdx), 1u);

				PushConsts pushConsts;
				pushConsts.texelSize = 1.0f / glm::vec2(mipWidth, mipHeight);
				pushConsts.width = mipWidth;
				pushConsts.height = mipHeight;
				pushConsts.addPrevious = i != 0 ? 1 : 0;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);
				
				cmdList->dispatch((mipWidth + 15) / 16, (mipHeight + 15) / 16, 1);


				// dont insert a barrier after the last iteration: we dont know the dstStage/dstAccess, so let the RenderGraph figure it out
				if (i < (BloomModule::BLOOM_MIP_COUNT - 2))
				{
					Barrier barrier = Initializers::imageBarrier(registry.getImage(data.m_resultImageViewHandles[curViewIdx]),
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						gal::ResourceState::WRITE_STORAGE_IMAGE,
						gal::ResourceState::READ_TEXTURE,
						{ curViewIdx, 1, 0, 1 });

					cmdList->barrier(1, &barrier);
				}
			}
		}, true);
}
