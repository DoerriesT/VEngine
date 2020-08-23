#include "ProbeDownsamplePass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

void VEngine::ProbeDownsamplePass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[RendererConsts::REFLECTION_PROBE_MIPS];

	passUsages[0] = { rg::ResourceViewHandle(data.m_resultImageViewHandles[0]), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
	for (uint32_t i = 1; i < RendererConsts::REFLECTION_PROBE_MIPS; ++i)
	{
		// last level isnt read from in this pass
		if (i == RendererConsts::REFLECTION_PROBE_MIPS - 1)
		{
			passUsages[i] = { rg::ResourceViewHandle(data.m_resultImageViewHandles[i]), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
		}
		else
		{
			passUsages[i] = { rg::ResourceViewHandle(data.m_resultImageViewHandles[i]), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }, true, {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
		}
	}

	graph.addPass("Reflection Probe Downsample", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/downsampleCubemap_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			uint32_t mipSize = RendererConsts::REFLECTION_PROBE_RES;

			for (uint32_t i = 1; i < RendererConsts::REFLECTION_PROBE_MIPS; ++i)
			{
				mipSize /= 2;

				// update descriptor set
				{
					DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

					ImageView *inputImageView = data.m_cubeImageViews[i - 1];
					ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandles[i]);

					DescriptorSetUpdate2 updates[] =
					{
						Initializers::rwTexture(&resultImageView, 1),
						Initializers::texture(&inputImageView, 0),
					};

					descriptorSet->update((uint32_t)std::size(updates), updates);

					DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
					cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
				}

				cmdList->dispatch((mipSize + 7) / 8, (mipSize + 7) / 8, 6);

				// dont insert a barrier after the last iteration: we dont know the next resource state, so let the RenderGraph figure it out
				if (i < RendererConsts::REFLECTION_PROBE_MIPS - 1)
				{
					Barrier barrier = Initializers::imageBarrier(registry.getImage(data.m_resultImageViewHandles[i]),
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						PipelineStageFlagBits::COMPUTE_SHADER_BIT,
						gal::ResourceState::WRITE_STORAGE_IMAGE,
						gal::ResourceState::READ_TEXTURE,
						{ i, 1, 0, 6 });

					cmdList->barrier(1, &barrier);
				}
			}
		}, true);
}
