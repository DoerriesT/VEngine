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
	rg::ResourceUsageDescription passUsages[8];

	passUsages[0] = { rg::ResourceViewHandle(data.m_inputImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
	for (size_t i = 0; i < 7; ++i)
	{
		passUsages[i + 1] = { rg::ResourceViewHandle(data.m_resultImageViewHandles[i]), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } };
	}

	graph.addPass("Reflection Probe Filter", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
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
				ImageView *resultImageView0 = registry.getImageView(data.m_resultImageViewHandles[0]);
				ImageView *resultImageView1 = registry.getImageView(data.m_resultImageViewHandles[1]);
				ImageView *resultImageView2 = registry.getImageView(data.m_resultImageViewHandles[2]);
				ImageView *resultImageView3 = registry.getImageView(data.m_resultImageViewHandles[3]);
				ImageView *resultImageView4 = registry.getImageView(data.m_resultImageViewHandles[4]);
				ImageView *resultImageView5 = registry.getImageView(data.m_resultImageViewHandles[5]);
				ImageView *resultImageView6 = registry.getImageView(data.m_resultImageViewHandles[6]);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView0, 0),
					Initializers::storageImage(&resultImageView1, 1),
					Initializers::storageImage(&resultImageView2, 2),
					Initializers::storageImage(&resultImageView3, 3),
					Initializers::storageImage(&resultImageView4, 4),
					Initializers::storageImage(&resultImageView5, 5),
					Initializers::storageImage(&resultImageView6, 6),
					Initializers::sampledImage(&inputImageView, 7),
					Initializers::samplerDescriptor(&linearSampler, 8),
					Initializers::sampledImage(&data.m_passRecordContext->m_renderResources->m_probeFilterCoeffsImageView, 9),
				};

				descriptorSet->update(10, updates);

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
		}, true);
}
