#include "GTAOSpatialFilterPass.h"
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
#include "../../../../Application/Resources/Shaders/hlsl/src/gtaoSpatialFilter.hlsli"
}


void VEngine::GTAOSpatialFilterPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_inputImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
	};

	graph.addPass("GTAO Spatial Filter", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/hlsl/gtaoSpatialFilter_cs.spv");
		
		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		cmdList->bindPipeline(pipeline);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *inputImageView = registry.getImageView(data.m_inputImageHandle);
			ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);

			DescriptorSetUpdate2 updates[] =
			{
				Initializers::texture(&inputImageView, INPUT_IMAGE_BINDING),
				Initializers::rwTexture(&resultImageView, RESULT_IMAGE_BINDING),
				Initializers::sampler(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
			};

			descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

		PushConsts pushConsts;
		pushConsts.texelSize = 1.0f / glm::vec2(width, height);
		pushConsts.width = width;
		pushConsts.height = height;

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
	});
}
