#include "VKLuminanceHistogramPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"

struct PushConsts
{
	float scale;
	float bias;
};


VEngine::VKLuminanceHistogramPass::VKLuminanceHistogramPass(VKRenderResources *renderResources, 
	uint32_t width, 
	uint32_t height, 
	size_t resourceIndex,
	float logMin,
	float logMax)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_logMin(logMin),
	m_logMax(logMax)
{
	strcpy_s(m_pipelineDesc.m_computeShaderPath, "Resources/Shaders/luminanceHistogram_comp.spv");

	m_pipelineDesc.m_layout.m_setLayoutCount = 2;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayouts[COMMON_SET_INDEX];
	m_pipelineDesc.m_layout.m_setLayouts[1] = m_renderResources->m_descriptorSetLayouts[LUMINANCE_HISTOGRAM_SET_INDEX];
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConsts) };
}

void VEngine::VKLuminanceHistogramPass::addToGraph(FrameGraph::Graph &graph, FrameGraph::ImageHandle lightTextureHandle, FrameGraph::BufferHandle luminanceHistogramBufferHandle)
{
	auto builder = graph.addComputePass("Luminance Histogram Pass", this, &m_pipelineDesc, FrameGraph::QueueType::GRAPHICS);

	// common set
	builder.writeStorageBuffer(luminanceHistogramBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::LUMINANCE_HISTOGRAM_BINDING);

	// histogram set
	builder.readTexture(lightTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSamplerClamp, m_renderResources->m_descriptorSets[m_resourceIndex][LUMINANCE_HISTOGRAM_SET_INDEX], LuminanceHistogramSetBindings::SOURCE_TEXTURE_BINDING);
}

void VEngine::VKLuminanceHistogramPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry & registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 1, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][LUMINANCE_HISTOGRAM_SET_INDEX], 0, nullptr);

	PushConsts pushConsts;
	pushConsts.scale = 1.0f / (m_logMax - m_logMin);
	pushConsts.bias = -m_logMin * pushConsts.scale;

	vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

	vkCmdDispatch(cmdBuf, m_height, 1, 1);
}
