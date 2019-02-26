#include "VKTilingPipeline.h"
#include "Graphics/Vulkan/VKShaderModule.h"
#include "Utility/Utility.h"
#include "Graphics/Vulkan/VKRenderResources.h"

VEngine::VKTilingPipeline::VKTilingPipeline(VkDevice device, VKRenderResources *renderResources)
	:m_device(device),
	m_pipeline(),
	m_pipelineLayout()
{
	VKShaderModule compShaderModule("Resources/Shaders/tiling_comp.spv");

	VkPipelineShaderStageCreateInfo compShaderStageInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = compShaderModule.get();
	compShaderStageInfo.pName = "main";

	VkDescriptorSetLayout layouts[] =
	{
		renderResources->m_perFrameDataDescriptorSetLayout,
		renderResources->m_lightBitMaskDescriptorSetLayout,
		renderResources->m_cullDataDescriptorSetLayout
	};

	VkPushConstantRange pushConstantRange;
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(uint32_t);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(sizeof(layouts) / sizeof(layouts[0]));
	pipelineLayoutInfo.pSetLayouts = layouts;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create pipeline layout!", -1);
	}

	VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	pipelineInfo.stage = compShaderStageInfo;
	pipelineInfo.layout = m_pipelineLayout;

	if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create pipeline!", -1);
	}
}

VEngine::VKTilingPipeline::~VKTilingPipeline()
{
	vkDestroyPipeline(m_device, m_pipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
}

VkPipeline VEngine::VKTilingPipeline::getPipeline() const
{
	return m_pipeline;
}

VkPipelineLayout VEngine::VKTilingPipeline::getLayout() const
{
	return m_pipelineLayout;
}
