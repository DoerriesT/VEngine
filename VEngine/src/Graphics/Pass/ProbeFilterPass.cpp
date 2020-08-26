#include "ProbeFilterPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

void VEngine::ProbeFilterPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	static_assert(RendererConsts::REFLECTION_PROBE_MIPS == 7);

	rg::ResourceUsageDescription passUsages[]
	{
		{ rg::ResourceViewHandle(data.m_inputImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } }
	};

	graph.addPass("Reflection Probe Filter", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// transition result image from READ_TEXTURE to WRITE_RW_IMAGE
			{
				gal::Barrier barrier = Initializers::imageBarrier(data.m_resultImageViews[0]->getImage(),
					PipelineStageFlagBits::COMPUTE_SHADER_BIT | PipelineStageFlagBits::FRAGMENT_SHADER_BIT,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					ResourceState::READ_TEXTURE,
					ResourceState::WRITE_RW_IMAGE,
					{ 0, 7, data.m_resultImageViews[0]->getDescription().m_baseArrayLayer, 6 });
				cmdList->barrier(1, &barrier);
			}

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/probeFilter_cs");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor set
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *inputImageView = registry.getImageView(data.m_inputImageViewHandle);

				DescriptorSetUpdate2 updates[] =
				{
					Initializers::rwTexture(&data.m_resultImageViews[0], 0),
					Initializers::rwTexture(&data.m_resultImageViews[1], 1),
					Initializers::rwTexture(&data.m_resultImageViews[2], 2),
					Initializers::rwTexture(&data.m_resultImageViews[3], 3),
					Initializers::rwTexture(&data.m_resultImageViews[4], 4),
					Initializers::rwTexture(&data.m_resultImageViews[5], 5),
					Initializers::rwTexture(&data.m_resultImageViews[6], 6),
					Initializers::texture(&inputImageView, 7),
					Initializers::texture(&data.m_probeFilterCoeffsImageView, 8),
				};

				descriptorSet->update((uint32_t)std::size(updates), updates);

				DescriptorSet *sets[]{ descriptorSet, data.m_passRecordContext->m_renderResources->m_computeSamplerDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, sets);
			}

			cmdList->dispatch(342, 6, 1);

			// transition result image from WRITE_RW_IMAGE to READ_TEXTURE
			{
				gal::Barrier barrier = Initializers::imageBarrier(data.m_resultImageViews[0]->getImage(),
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT | PipelineStageFlagBits::FRAGMENT_SHADER_BIT,
					ResourceState::WRITE_RW_IMAGE,
					ResourceState::READ_TEXTURE,
					{ 0, 7, data.m_resultImageViews[0]->getDescription().m_baseArrayLayer, 6 });
				cmdList->barrier(1, &barrier);
			}
		}, true);
}
