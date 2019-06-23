#include "VKGTAOTemporalFilterPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "GlobalVar.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/gtaoTemporalFilter_bindings.h"
}

void VEngine::VKGTAOTemporalFilterPass::addToGraph(FrameGraph::Graph & graph, const Data & data)
{
	graph.addComputePass("GTAO Temporal Filter Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readTexture(data.m_inputImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_velocityImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_previousImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		builder.writeStorageImage(data.m_resultImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/gtaoTemporalFilter_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkSampler pointSamplerClamp = data.m_renderResources->m_pointSamplerClamp;

			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// input image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_inputImageHandle, pointSamplerClamp), INPUT_IMAGE_BINDING);

			// velocity image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_velocityImageHandle, pointSamplerClamp), VELOCITY_IMAGE_BINDING);

			// previous image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_previousImageHandle, data.m_renderResources->m_linearSamplerClamp), PREVIOUS_IMAGE_BINDING);

			// result image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_resultImageHandle), RESULT_IMAGE_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		glm::mat4 invViewProjection = data.m_invViewProjection;
		//assert(invViewProjection[0][3] == 0.0f);
		//assert(invViewProjection[1][3] == 0.0f);
		//invViewProjection[0][3] = data.m_nearPlane;
		//invViewProjection[1][3] = data.m_farPlane;

		glm::mat4 prevInvViewProjection = data.m_prevInvViewProjection;
		//assert(prevInvViewProjection[0][3] == 0.0f);
		//assert(prevInvViewProjection[1][3] == 0.0f);
		//prevInvViewProjection[0][3] = data.m_nearPlane;
		//prevInvViewProjection[1][3] = data.m_farPlane;

		PushConsts pushConsts;
		pushConsts.invViewProjectionNearFar = invViewProjection;
		pushConsts.prevInvViewProjectionNearFar = prevInvViewProjection;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		VKUtility::dispatchComputeHelper(cmdBuf, data.m_width, data.m_height, 1, 8, 8, 1);
	});
}
