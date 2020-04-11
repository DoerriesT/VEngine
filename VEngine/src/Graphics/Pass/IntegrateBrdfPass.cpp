#include "IntegrateBrdfPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
//#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/integrateBrdf.hlsli"
}

void VEngine::IntegrateBrdfPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Integrate BRDF", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = 128;
			const uint32_t height = 128;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/hlsl/integrateBrdf_cs.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));
				ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);

				DescriptorSetUpdate update = Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING);

				descriptorSet->update(1, &update);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			PushConsts pushConsts;
			pushConsts.width = width;
			pushConsts.height = height;
			pushConsts.texelWidth = 1.0f / width;
			pushConsts.texelHeight = 1.0f / height;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		}, true);
}
