#include "SwapChainCopyPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

void VEngine::SwapChainCopyPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_inputImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Copy SwapChain", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/swapChainCopy_cs");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *inputImageView = registry.getImageView(data.m_inputImageViewHandle);

				DescriptorSetUpdate2 updates[] =
				{
					Initializers::rwTexture(&data.m_resultImageView, 0),
					Initializers::texture(&inputImageView, 1),
				};

				descriptorSet->update((uint32_t)std::size(updates), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			// barrier to transition result image from TEXTURE to STORAGE
			{
				Barrier barrier = Initializers::imageBarrier(data.m_resultImageView->getImage(),
					PipelineStageFlagBits::FRAGMENT_SHADER_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					gal::ResourceState::READ_TEXTURE, gal::ResourceState::WRITE_RW_IMAGE);

				cmdList->barrier(1, &barrier);
			}

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);

			// barrier to transition result image from STORAGE to TEXTURE
			{
				Barrier barrier = Initializers::imageBarrier(data.m_resultImageView->getImage(),
					PipelineStageFlagBits::COMPUTE_SHADER_BIT, PipelineStageFlagBits::FRAGMENT_SHADER_BIT,
					gal::ResourceState::WRITE_RW_IMAGE, gal::ResourceState::READ_TEXTURE);

				cmdList->barrier(1, &barrier);
			}
		}, true);
}
