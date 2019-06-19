#include "VKTAAResolvePass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "GlobalVar.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/VKContext.h"
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace
{
	using vec4 = glm::vec4;
	using mat4 = glm::mat4;
	using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/taaResolve_bindings.h"
}

void VEngine::VKTAAResolvePass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addComputePass("TAA Resolve Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readTexture(data.m_depthImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_velocityImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_taaHistoryImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_lightImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		builder.writeStorageImage(data.m_taaResolveImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/taaResolve_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkSampler pointSamplerClamp = data.m_renderResources->m_pointSamplerClamp;
			VkSampler linearSamplerClamp = data.m_renderResources->m_linearSamplerClamp;

			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// result image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, registry.getImageInfo(data.m_taaResolveImageHandle), RESULT_IMAGE_BINDING);

			// depth image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_depthImageHandle, pointSamplerClamp), DEPTH_IMAGE_BINDING);

			// velocity image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_velocityImageHandle, pointSamplerClamp), VELOCITY_IMAGE_BINDING);

			// history image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_taaHistoryImageHandle, linearSamplerClamp), HISTORY_IMAGE_BINDING);

			// source image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_lightImageHandle, linearSamplerClamp), SOURCE_IMAGE_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		PushConsts pushConsts;
		pushConsts.bicubicSharpness = g_TAABicubicSharpness;
		pushConsts.temporalContrastThreshold = g_TAATemporalContrastThreshold;
		pushConsts.lowStrengthAlpha = g_TAALowStrengthAlpha;
		pushConsts.highStrengthAlpha = g_TAAHighStrengthAlpha;
		pushConsts.antiFlickeringAlpha = g_TAAAntiFlickeringAlpha;
		pushConsts.jitterOffsetWeight = exp(-2.29f * (data.m_jitterOffsetX * data.m_jitterOffsetX + data.m_jitterOffsetY * data.m_jitterOffsetY));

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		VKUtility::dispatchComputeHelper(cmdBuf, data.m_width, data.m_height, 1, 8, 8, 1);
	});
}
