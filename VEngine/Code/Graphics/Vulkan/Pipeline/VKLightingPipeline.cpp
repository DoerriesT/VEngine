#include "VKLightingPipeline.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKShaderModule.h"
#include "Utility/Utility.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKRenderResources.h"

VEngine::VKLightingPipeline::VKLightingPipeline()
{
}

VEngine::VKLightingPipeline::~VKLightingPipeline()
{
}

void VEngine::VKLightingPipeline::init(VKRenderResources * renderResources)
{
	VKShaderModule compShaderModule("Resources/Shaders/lighting_comp.spv");

	VkPipelineShaderStageCreateInfo compShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = compShaderModule.get();
	compShaderStageInfo.pName = "main";

	VkDescriptorSetLayout layouts[] =
	{
		renderResources->m_lightingInputDescriptorSetLayout,
		renderResources->m_perFrameDataDescriptorSetLayout,
		renderResources->m_lightDataDescriptorSetLayout,
		renderResources->m_lightIndexDescriptorSetLayout
	};

	VkPushConstantRange pushConstantRange;
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(uint32_t);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(sizeof(layouts) / sizeof(layouts[0]));;
	pipelineLayoutInfo.pSetLayouts = layouts;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

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

void VEngine::VKLightingPipeline::recordCommandBuffer(VKRenderResources *renderResources, uint32_t width, uint32_t height)
{
	vkCmdBindPipeline(renderResources->m_lightingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

	vkCmdBindDescriptorSets(renderResources->m_lightingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &renderResources->m_lightingInputDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(renderResources->m_lightingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 1, 1, &renderResources->m_perFrameDataDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(renderResources->m_lightingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 2, 1, &renderResources->m_lightDataDescriptorSet, 0, nullptr);
	vkCmdBindDescriptorSets(renderResources->m_lightingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 3, 1, &renderResources->m_lightIndexDescriptorSet, 0, nullptr);
	
	uint32_t directionalLightCount = 1;
	vkCmdPushConstants(renderResources->m_lightingCommandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &directionalLightCount);

	// wait for shadows
	vkCmdWaitEvents(renderResources->m_lightingCommandBuffer, 1, &renderResources->m_shadowsFinishedEvent, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, nullptr, 0, nullptr, 0, nullptr);

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	VKUtility::setImageLayout(
		renderResources->m_lightingCommandBuffer,
		renderResources->m_lightAttachment.getImage(),
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		subresourceRange,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	VKUtility::dispatchComputeHelper(renderResources->m_lightingCommandBuffer, width, height, 1, 8, 8, 1);
}
