#include "GTAOPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "GlobalVar.h"
#include <glm/detail/func_trigonometric.hpp>
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
#include "../../../../Application/Resources/Shaders/hlsl/src/hlslToGlm.h"
#include "../../../../Application/Resources/Shaders/hlsl/src/gtao.hlsli"
}

void VEngine::GTAOPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_depthImageHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT} },
		//{rg::ResourceViewHandle(data.m_tangentSpaceImageHandle), {ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT }},
	};

	graph.addPass("GTAO", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/hlsl/gtao_cs.spv");
		
		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		cmdList->bindPipeline(pipeline);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *depthImageView = registry.getImageView(data.m_depthImageHandle);
			//ImageView *tangentSpaceImageView = registry.getImageView(data.m_tangentSpaceImageHandle);
			ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);

			DescriptorSetUpdate updates[] =
			{
				Initializers::sampledImage(&depthImageView, DEPTH_IMAGE_BINDING),
				Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
				Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
			};

			descriptorSet->update(sizeof(updates) / sizeof(updates[0]), updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

		PushConsts pushConsts;
		pushConsts.invProjection = data.m_passRecordContext->m_commonRenderData->m_invProjectionMatrix;
		pushConsts.resolution = glm::vec4(width, height, 1.0f / width, 1.0f / height);
		pushConsts.focalLength = 1.0f / tanf(glm::radians(data.m_passRecordContext->m_commonRenderData->m_fovy) * 0.5f) * (height / static_cast<float>(width));
		pushConsts.radius = g_gtaoRadius;
		pushConsts.maxRadiusPixels = static_cast<float>(g_gtaoMaxRadiusPixels);
		pushConsts.numSteps = static_cast<float>(g_gtaoSteps);
		pushConsts.frame = data.m_passRecordContext->m_commonRenderData->m_frame;

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
	});
}
