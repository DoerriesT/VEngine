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
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/gtaoTemporalFilter_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[4] = {};

			// input image
			VkDescriptorImageInfo inputImageInfo = registry.getImageInfo(data.m_inputImageHandle);
			inputImageInfo.sampler = data.m_renderResources->m_pointSamplerClamp;
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = INPUT_IMAGE_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[0].pImageInfo = &inputImageInfo;

			// velocity image
			VkDescriptorImageInfo velocityImageInfo = registry.getImageInfo(data.m_velocityImageHandle);
			velocityImageInfo.sampler = data.m_renderResources->m_pointSamplerClamp;
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = VELOCITY_IMAGE_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].pImageInfo = &velocityImageInfo;

			// previous image
			VkDescriptorImageInfo previousImageInfo = registry.getImageInfo(data.m_previousImageHandle);
			previousImageInfo.sampler = data.m_renderResources->m_linearSamplerClamp;
			descriptorWrites[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[2].dstSet = descriptorSet;
			descriptorWrites[2].dstBinding = PREVIOUS_IMAGE_BINDING;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorCount = 1;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[2].pImageInfo = &previousImageInfo;

			// result image
			VkDescriptorImageInfo resultImageInfo = registry.getImageInfo(data.m_resultImageHandle);
			descriptorWrites[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[3].dstSet = descriptorSet;
			descriptorWrites[3].dstBinding = RESULT_IMAGE_BINDING;
			descriptorWrites[3].dstArrayElement = 0;
			descriptorWrites[3].descriptorCount = 1;
			descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptorWrites[3].pImageInfo = &resultImageInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
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
