#include "VKTilingPipeline.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKShaderModule.h"
#include "Utility/Utility.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKRenderResources.h"

VEngine::VKTilingPipeline::VKTilingPipeline()
{
}

VEngine::VKTilingPipeline::~VKTilingPipeline()
{
}

void VEngine::VKTilingPipeline::init(VKRenderResources *renderResources)
{
	VKShaderModule compShaderModule("Resources/Shaders/tiling_comp.spv");

	VkPipelineShaderStageCreateInfo compShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = compShaderModule.get();
	compShaderStageInfo.pName = "main";

	VkDescriptorSetLayout layouts[] =
	{
		renderResources->m_perFrameDataDescriptorSetLayout,
		renderResources->m_lightIndexDescriptorSetLayout,
		renderResources->m_cullDataDescriptorSetLayout
	};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(sizeof(layouts) / sizeof(layouts[0]));
	pipelineLayoutInfo.pSetLayouts = layouts;

	if (vkCreatePipelineLayout(g_context.m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create pipeline layout!", -1);
	}

	VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	pipelineInfo.stage = compShaderStageInfo;
	pipelineInfo.layout = m_pipelineLayout;

	if (vkCreateComputePipelines(g_context.m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create pipeline!", -1);
	}
}

void VEngine::VKTilingPipeline::recordCommandBuffer(VKRenderResources *renderResources, uint32_t width, uint32_t height)
{
	vkCmdBindPipeline(renderResources->m_tilingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

	vkCmdBindDescriptorSets(renderResources->m_tilingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &renderResources->m_perFrameDataDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(renderResources->m_tilingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 1, 1, &renderResources->m_lightIndexDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(renderResources->m_tilingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 2, 1, &renderResources->m_cullDataDescriptorSet, 0, nullptr);
	
	//width = width / 16 + ((width % 16 == 0) ? 0 : 1);
	//height = height / 16 + ((height % 16 == 0) ? 0 : 1);
	VKUtility::dispatchComputeHelper(renderResources->m_tilingCommandBuffer, width, height, 1, 16, 16, 1);
}
