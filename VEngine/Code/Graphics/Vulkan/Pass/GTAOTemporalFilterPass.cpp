#include "GTAOTemporalFilterPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "GlobalVar.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/gtaoTemporalFilter_bindings.h"
}

void VEngine::GTAOTemporalFilterPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_resultImageHandle), ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_inputImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_velocityImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
		{ResourceViewHandle(data.m_previousImageHandle), ResourceState::READ_TEXTURE_COMPUTE_SHADER},
	};

	graph.addPass("GTAO Temporal Filter", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		ComputePipelineDesc pipelineDesc;
		pipelineDesc.setComputeShader("Resources/Shaders/gtaoTemporalFilter_comp.spv");
		pipelineDesc.finalize();

		auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];

			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_inputImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), INPUT_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_velocityImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, pointSamplerClamp), VELOCITY_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_previousImageHandle, ResourceState::READ_TEXTURE_COMPUTE_SHADER, data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_CLAMP_IDX]), PREVIOUS_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_resultImageHandle, ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER), RESULT_IMAGE_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

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

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		VKUtility::dispatchComputeHelper(cmdBuf, width, height, 1, 8, 8, 1);
	});
}
