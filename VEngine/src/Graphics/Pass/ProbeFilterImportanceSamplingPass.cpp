#include "ProbeFilterImportanceSamplingPass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/probeFilterImportanceSampling.hlsli"
}

void VEngine::ProbeFilterImportanceSamplingPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{ rg::ResourceViewHandle(data.m_inputImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT } }
	};

	graph.addPass("Reflection Probe Filter IS", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// transition result image from READ_TEXTURE to WRITE_STORAGE_IMAGE
			{
				gal::Barrier barrier = Initializers::imageBarrier(data.m_resultImageViews[0]->getImage(),
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					ResourceState::READ_TEXTURE,
					ResourceState::WRITE_STORAGE_IMAGE,
					{ 0, RendererConsts::REFLECTION_PROBE_MIPS, data.m_resultImageViews[0]->getDescription().m_baseArrayLayer, 6 });
				cmdList->barrier(1, &barrier);
			}

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/probeFilterImportanceSampling_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			Sampler *linearSampler = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX];

			// update descriptor set
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *inputImageView = registry.getImageView(data.m_inputImageViewHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(data.m_resultImageViews, RESULT_IMAGE_BINDING, 0, RendererConsts::REFLECTION_PROBE_MIPS),
					Initializers::sampledImage(&inputImageView, INPUT_IMAGE_BINDING),
					Initializers::samplerDescriptor(&linearSampler, LINEAR_SAMPLER_BINDING),
				};

				descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			uint32_t mipSize = RendererConsts::REFLECTION_PROBE_RES;
			for (uint32_t i = 0; i < RendererConsts::REFLECTION_PROBE_MIPS; ++i)
			{
				PushConsts pushConsts;
				pushConsts.mip = i;
				pushConsts.width = mipSize;
				pushConsts.mipCount = RendererConsts::REFLECTION_PROBE_MIPS;
				pushConsts.texelSize = 1.0f / mipSize;
				pushConsts.roughness = i / float(RendererConsts::REFLECTION_PROBE_MIPS);
				pushConsts.roughness *= pushConsts.roughness;

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);
				cmdList->dispatch((pushConsts.width + 7) / 8, (pushConsts.width + 7) / 8, 6);

				mipSize /= 2;
			}

			// transition result image from WRITE_STORAGE_IMAGE to READ_TEXTURE
			{
				gal::Barrier barrier = Initializers::imageBarrier(data.m_resultImageViews[0]->getImage(),
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					ResourceState::WRITE_STORAGE_IMAGE,
					ResourceState::READ_TEXTURE,
					{ 0, RendererConsts::REFLECTION_PROBE_MIPS, data.m_resultImageViews[0]->getDescription().m_baseArrayLayer, 6 });
				cmdList->barrier(1, &barrier);
			}
		}, true);
}
