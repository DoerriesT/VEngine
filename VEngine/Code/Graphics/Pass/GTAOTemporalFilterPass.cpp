#include "GTAOTemporalFilterPass.h"
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
	using namespace glm;
#include "../../../../Application/Resources/Shaders/gtaoTemporalFilter_bindings.h"
}

void VEngine::GTAOTemporalFilterPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageHandle), {gal::ResourceState::WRITE_STORAGE_IMAGE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_inputImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_velocityImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(data.m_previousImageHandle), {gal::ResourceState::READ_TEXTURE, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("GTAO Temporal Filter", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineCreateInfo pipelineCreateInfo;
		ComputePipelineBuilder builder(pipelineCreateInfo);
		builder.setComputeShader("Resources/Shaders/gtaoTemporalFilter_comp.spv");
		
		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		cmdList->bindPipeline(pipeline);

		// update descriptor sets
		{
			DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

			ImageView *inputImageView = registry.getImageView(data.m_inputImageHandle);
			ImageView *velocityImageView = registry.getImageView(data.m_velocityImageHandle);
			ImageView *prevImageView = registry.getImageView(data.m_previousImageHandle);
			ImageView *resultImageView = registry.getImageView(data.m_resultImageHandle);

			DescriptorSetUpdate updates[] =
			{
				Initializers::sampledImage(&inputImageView, INPUT_IMAGE_BINDING),
				Initializers::sampledImage(&velocityImageView, VELOCITY_IMAGE_BINDING),
				Initializers::sampledImage(&prevImageView, PREVIOUS_IMAGE_BINDING),
				Initializers::storageImage(&resultImageView, RESULT_IMAGE_BINDING),
				Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX], POINT_SAMPLER_BINDING),
				Initializers::samplerDescriptor(&data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX], LINEAR_SAMPLER_BINDING),
			};

			descriptorSet->update(6, updates);

			cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
		}

		glm::mat4 invViewProjection = data.m_passRecordContext->m_commonRenderData->m_invViewProjectionMatrix;
		//assert(invViewProjection[0][3] == 0.0f);
		//assert(invViewProjection[1][3] == 0.0f);
		//invViewProjection[0][3] = data.m_nearPlane;
		//invViewProjection[1][3] = data.m_farPlane;

		glm::mat4 prevInvViewProjection = data.m_passRecordContext->m_commonRenderData->m_prevInvViewProjectionMatrix;
		//assert(prevInvViewProjection[0][3] == 0.0f);
		//assert(prevInvViewProjection[1][3] == 0.0f);
		//prevInvViewProjection[0][3] = data.m_nearPlane;
		//prevInvViewProjection[1][3] = data.m_farPlane;

		PushConsts pushConsts;
		pushConsts.invViewProjectionNearFar = invViewProjection;
		pushConsts.prevInvViewProjectionNearFar = prevInvViewProjection;

		cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		cmdList->dispatch((width + 7) / 8, (height + 7) / 8, 1);
	});
}
