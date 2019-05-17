#include "VKFXAAPass.h"
#include <glm/vec2.hpp>
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "GlobalVar.h"

namespace
{
	using vec2 = glm::vec2;
#include "../../../../../Application/Resources/Shaders/fxaa_bindings.h"
}

void VEngine::VKFXAAPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addComputePass("FXAA Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readTexture(data.m_inputImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		builder.writeStorageImage(data.m_resultImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/fxaa_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[2] = {};

			// input image
			VkDescriptorImageInfo inputImageInfo = registry.getImageInfo(data.m_inputImageHandle);
			inputImageInfo.sampler = data.m_renderResources->m_linearSamplerClamp;
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = INPUT_IMAGE_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[0].pImageInfo = &inputImageInfo;

			// result image
			VkDescriptorImageInfo resultImageInfo = registry.getImageInfo(data.m_resultImageHandle);
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = RESULT_IMAGE_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptorWrites[1].pImageInfo = &resultImageInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		PushConsts pushConsts;
		pushConsts.invScreenSizeInPixels = 1.0f / glm::vec2(data.m_width, data.m_height);
		pushConsts.fxaaQualitySubpix = g_FXAAQualitySubpix;
		pushConsts.fxaaQualityEdgeThreshold = g_FXAAQualityEdgeThreshold;
		pushConsts.fxaaQualityEdgeThresholdMin = g_FXAAQualityEdgeThresholdMin;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		VKUtility::dispatchComputeHelper(cmdBuf, data.m_width, data.m_height, 1, 8, 8, 1);
	});
}