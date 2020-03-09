#include "FXAAPass.h"
#include <glm/vec2.hpp>
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "GlobalVar.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	using vec2 = glm::vec2;
#include "../../../../Application/Resources/Shaders/fxaa_bindings.h"
}

void VEngine::FXAAPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_inputImageHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
	};

	graph.addPass("FXAA", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/fxaa_comp.spv");

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		cmdList->bindPipeline(pipeline);

		
		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *inputImageView = registry.getImageView(data.m_inputImageHandle);
			ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);

			DescriptorSetUpdate updates[] =
			{
				Initializers::sampledImage(&inputImageView, INPUT_IMAGE_BINDING),
				Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
				Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
			};

			descriptorSet->update(3, updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

		PushConsts pushConsts;
		pushConsts.invScreenSizeInPixels = 1.0f / glm::vec2(width, height);
		pushConsts.fxaaQualitySubpix = g_FXAAQualitySubpix;
		pushConsts.fxaaQualityEdgeThreshold = g_FXAAQualityEdgeThreshold;
		pushConsts.fxaaQualityEdgeThresholdMin = g_FXAAQualityEdgeThresholdMin;

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
	});
}
