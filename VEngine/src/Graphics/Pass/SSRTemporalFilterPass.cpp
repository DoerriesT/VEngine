#include "SSRTemporalFilterPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Utility/Utility.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	using namespace glm;
#include "../../../../Application/Resources/Shaders/ssrTemporalFilter_bindings.h"
}

void VEngine::SSRTemporalFilterPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_historyImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_colorRayDepthImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_maskImageViewHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("SSR Temporal Filter", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/ssrTemporalFilter_comp.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				ImageView *resultImageView = registry.getImageView(data.m_resultImageViewHandle);
				ImageView *historyImageView = registry.getImageView(data.m_historyImageViewHandle);
				ImageView *colorRayDepthImageView = registry.getImageView(data.m_colorRayDepthImageViewHandle);
				ImageView *maskImageView = registry.getImageView(data.m_maskImageViewHandle);

				DescriptorSetUpdate updates[] =
				{
					Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
					Initializers::sampledImage(&historyImageView, HISTORY_IMAGE_BINDING),
					Initializers::sampledImage(&colorRayDepthImageView, COLOR_RAY_DEPTH_IMAGE_BINDING),
					Initializers::sampledImage(&maskImageView, MASK_IMAGE_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
					Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
				};

				descriptorSet->update(6, updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			const glm::mat4 reprojectionMatrix = data.m_passRecordContext->m_commonRenderData->m_prevViewProjectionMatrix * data.m_passRecordContext->m_commonRenderData->m_invViewProjectionMatrix;

			PushConsts pushConsts;
			pushConsts.reprojectionMatrix = reprojectionMatrix;
			pushConsts.width = static_cast<float>(width);
			pushConsts.height = static_cast<float>(height);
			pushConsts.texelWidth = 1.0f / pushConsts.width;
			pushConsts.texelHeight = 1.0f / pushConsts.height;

			cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
		});
}
