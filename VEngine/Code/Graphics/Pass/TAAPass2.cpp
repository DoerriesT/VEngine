#include "TAAPass2.h"
#include "Graphics/RenderResources.h"
#include "GlobalVar.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext2.h"
#include "Graphics/RenderData.h"
#include <glm/vec3.hpp>
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	using namespace glm;
#include "../../../../Application/Resources/Shaders/taa_bindings.h"
}

void VEngine::TAAPass2::addToGraph(rg::RenderGraph2 &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_depthImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_velocityImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_taaHistoryImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_lightImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_taaResolveImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("TAA Resolve", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry2 &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/taa_comp.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = registry.getImageView(data.m_taaResolveImageHandle);
				ImageView *depthImageView = registry.getImageView(data.m_depthImageHandle);
				ImageView *velocityImageView = registry.getImageView(data.m_velocityImageHandle);
				ImageView *historyImageView = registry.getImageView(data.m_taaHistoryImageHandle);
				ImageView *lightImageView = registry.getImageView(data.m_lightImageHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&velocityImageView, VELOCITY_IMAGE_BINDING),
					Initializers::sampledImage(&historyImageView, HISTORY_IMAGE_BINDING),
					Initializers::sampledImage(&lightImageView, SOURCE_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
				};

				descriptorSet->update(7, updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			const float jitterOffsetLength = glm::length(glm::vec2(data.m_jitterOffsetX * width, data.m_jitterOffsetY * height));

			PushConsts pushConsts;
			pushConsts.bicubicSharpness = g_TAABicubicSharpness;
			pushConsts.temporalContrastThreshold = g_TAATemporalContrastThreshold;
			pushConsts.lowStrengthAlpha = g_TAALowStrengthAlpha;
			pushConsts.highStrengthAlpha = g_TAAHighStrengthAlpha;
			pushConsts.antiFlickeringAlpha = g_TAAAntiFlickeringAlpha;
			pushConsts.jitterOffsetWeight = exp(-2.29f * jitterOffsetLength);

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		});
}
