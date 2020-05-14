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
	rg::ResourceUsageDescription passUsages[]
	{
		{ rg::ResourceViewHandle(data.m_inputImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } }
	};

	graph.addPass("Reflection Probe Filter", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// transition image from READ_TEXTURE to WRITE_STORAGE_IMAGE
			{
				gal::Barrier barrier = Initializers::imageBarrier(data.m_resultImageViews[0]->getImage(),
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					ResourceState::READ_TEXTURE,
					ResourceState::WRITE_STORAGE_IMAGE,
					{ 0, 7, data.m_resultImageViews[0]->getDescription().m_baseArrayLayer, 6 });
				cmdList->barrier(1, &barrier);
			}

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/probeFilter_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			Sampler *linearSampler = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX];

			// update descriptor set
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *inputImageView = registry.getImageView(data.m_inputImageViewHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&data.m_resultImageViews[0], 0),
					Initializers::storageImage(&data.m_resultImageViews[1], 1),
					Initializers::storageImage(&data.m_resultImageViews[2], 2),
					Initializers::storageImage(&data.m_resultImageViews[3], 3),
					Initializers::storageImage(&data.m_resultImageViews[4], 4),
					Initializers::storageImage(&data.m_resultImageViews[5], 5),
					Initializers::storageImage(&data.m_resultImageViews[6], 6),
					Initializers::sampledImage(&inputImageView, 7),
					Initializers::samplerDescriptor(&linearSampler, 8),
					Initializers::sampledImage(&data.m_passRecordContext->m_renderResources->m_probeFilterCoeffsImageView, 9),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			uint32_t texelCount = 0;
			{
				uint32_t s = 128;
				for (size_t i = 0; i < 7; ++i)
				{
					texelCount += s * s;
					s /= 2;
				}
			}

			cmdList->dispatch((texelCount + 63) / 64 + 1, 6, 1);

			// transition image from WRITE_STORAGE_IMAGE to READ_TEXTURE
			{
				gal::Barrier barrier = Initializers::imageBarrier(data.m_resultImageViews[0]->getImage(),
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					ResourceState::WRITE_STORAGE_IMAGE,
					ResourceState::READ_TEXTURE,
					{ 0, 7, data.m_resultImageViews[0]->getDescription().m_baseArrayLayer, 6 });
				cmdList->barrier(1, &barrier);
			}
		}, true);
}
