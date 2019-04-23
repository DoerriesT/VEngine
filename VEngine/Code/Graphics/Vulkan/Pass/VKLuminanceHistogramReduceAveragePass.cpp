#include "VKLuminanceHistogramReduceAveragePass.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/luminanceHistogramReduceAverage_bindings.h"

void VEngine::VKLuminanceHistogramAveragePass::addToGraph(FrameGraph::Graph & graph, const Data & data)
{
	graph.addComputePass("Luminance Histogram Average Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readWriteStorageBuffer(data.m_luminanceHistogramBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readWriteStorageBuffer(data.m_avgLuminanceBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/luminanceHistogramReduceAverage_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[2] = {};

			// histogram
			VkDescriptorBufferInfo histogramBufferInfo = registry.getBufferInfo(data.m_luminanceHistogramBufferHandle);
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = LUMINANCE_HISTOGRAM_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[0].pBufferInfo = &histogramBufferInfo;

			// avg luminance values
			VkDescriptorBufferInfo avgLuminanceBufferInfo = registry.getBufferInfo(data.m_avgLuminanceBufferHandle);
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = LUMINANCE_VALUES_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[1].pBufferInfo = &avgLuminanceBufferInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		const float lowerBoundPercentage = 0.5f;
		const float upperBoundPercentage = 0.9f;

		PushConsts pushConsts;
		pushConsts.precomputedTerm = 1.0f - exp(-data.m_timeDelta * 1.1f);
		pushConsts.invScale = data.m_logMax - data.m_logMin;
		pushConsts.bias = -data.m_logMin * (1.0f / pushConsts.invScale);
		pushConsts.lowerBound = static_cast<uint32_t>(data.m_width * data.m_height * lowerBoundPercentage);
		pushConsts.upperBound = static_cast<uint32_t>(data.m_width * data.m_height * upperBoundPercentage);
		pushConsts.currentResourceIndex = data.m_currentResourceIndex;
		pushConsts.previousResourceIndex = data.m_previousResourceIndex;


		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		vkCmdDispatch(cmdBuf, 1, 1, 1);
	});
}
