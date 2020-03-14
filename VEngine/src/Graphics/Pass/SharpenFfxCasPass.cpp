#include "SharpenFfxCasPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	using namespace glm;
#include "../../../../Application/Resources/Shaders/sharpen_ffx_cas_bindings.h"
}

#define A_CPU 1
#include "../../../../Application/Resources/Shaders/ffx_a.h"
#include "../../../../Application/Resources/Shaders/ffx_cas.h"


void VEngine::SharpenFfxCasPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_srcImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_dstImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Sharpen (FFX CAS)", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/sharpen_ffx_cas_comp.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = registry.getImageView(data.m_dstImageHandle);
				ImageView *inputImageView = registry.getImageView(data.m_srcImageHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::sampledImage(&inputImageView, SOURCE_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
				};

				descriptorSet->update(3, updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			glm::uvec4 const0;
			glm::uvec4 const1;
			CasSetup(&const0[0], &const1[0], data.m_sharpness, AF1(width), AF1(height), AF1(width), AF1(height));

			PushConsts pushConsts;
			pushConsts.const0 = const0;
			pushConsts.const1 = const1;
			pushConsts.gammaSpaceInput = data.m_gammaSpaceInput;
			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		});
}
