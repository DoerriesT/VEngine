#include "VKTonemapPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"

VEngine::VKTonemapPass::VKTonemapPass(VKRenderResources *renderResources, 
	uint32_t width, 
	uint32_t height, 
	size_t resourceIndex)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex)
{
	strcpy_s(m_pipelineDesc.m_computeShaderPath, "Resources/Shaders/tonemap_comp.spv");

	m_pipelineDesc.m_layout.m_setLayoutCount = 2;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayouts[COMMON_SET_INDEX];
	m_pipelineDesc.m_layout.m_setLayouts[1] = m_renderResources->m_descriptorSetLayouts[TONEMAP_SET_INDEX];
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) };
}

void VEngine::VKTonemapPass::addToGraph(FrameGraph::Graph &graph, FrameGraph::ImageHandle srcImageHandle, FrameGraph::ImageHandle dstImageHandle, FrameGraph::BufferHandle avgLuminanceBufferHandle)
{
	auto builder = graph.addComputePass("Tonemap Pass", this, &m_pipelineDesc, FrameGraph::QueueType::GRAPHICS);

	// common set
	builder.readStorageBuffer(avgLuminanceBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::PERSISTENT_VALUES_BINDING);

	// tonemap set
	builder.readTexture(srcImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler, m_renderResources->m_descriptorSets[m_resourceIndex][TONEMAP_SET_INDEX], TonemapSetBindings::SOURCE_TEXTURE_BINDING);
	builder.writeStorageImage(dstImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][TONEMAP_SET_INDEX], TonemapSetBindings::RESULT_IMAGE_BINDING);
}

void VEngine::VKTonemapPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry & registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 1, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][TONEMAP_SET_INDEX], 0, nullptr);

	uint32_t luminanceIndex = static_cast<uint32_t>(m_resourceIndex);
	vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(luminanceIndex), &luminanceIndex);

	VKUtility::dispatchComputeHelper(cmdBuf, m_width, m_height, 1, 8, 8, 1);
}
