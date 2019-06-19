#include "VKLuminanceHistogramPass.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace
{
	using vec4 = glm::vec4;
	using mat4 = glm::mat4;
	using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/luminanceHistogram_bindings.h"
}

void VEngine::VKLuminanceHistogramPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addComputePass("Luminance Histogram Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readTexture(data.m_lightImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		builder.writeStorageBuffer(data.m_luminanceHistogramBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/luminanceHistogram_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// source image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_lightImageHandle, data.m_renderResources->m_pointSamplerClamp), SOURCE_IMAGE_BINDING);

			// histogram
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_luminanceHistogramBufferHandle), LUMINANCE_HISTOGRAM_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		PushConsts pushConsts;
		pushConsts.scale = 1.0f / (data.m_logMax - data.m_logMin);
		pushConsts.bias = -data.m_logMin * pushConsts.scale;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		vkCmdDispatch(cmdBuf, data.m_height, 1, 1);
	});
}
