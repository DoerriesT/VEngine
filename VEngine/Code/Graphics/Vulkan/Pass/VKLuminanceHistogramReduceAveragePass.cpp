#include "VKLuminanceHistogramReduceAveragePass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include <iostream>

struct PushConsts
{
	float precomputedTerm;
	uint32_t lowerBound;
	uint32_t upperBound;
	float invScale;
	float bias;
};

VEngine::VKLuminanceHistogramReduceAveragePass::VKLuminanceHistogramReduceAveragePass(VKRenderResources *renderResources, 
	uint32_t width, 
	uint32_t height, 
	size_t resourceIndex, 
	float timeDelta,
	float logMin,
	float logMax)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_timeDelta(timeDelta),
	m_logMin(logMin),
	m_logMax(logMax)
{
	strcpy_s(m_pipelineDesc.m_computeShaderPath, "Resources/Shaders/luminanceHistogramReduceAverage_comp.spv");

	m_pipelineDesc.m_layout.m_setLayoutCount = 1;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayouts[COMMON_SET_INDEX];
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConsts) };
}

void VEngine::VKLuminanceHistogramReduceAveragePass::addToGraph(FrameGraph::Graph & graph, FrameGraph::BufferHandle luminanceHistogramBufferHandle, FrameGraph::BufferHandle avgLuminanceBufferHandle)
{
	auto builder = graph.addComputePass("Luminance Histogram Reduce/Average Pass", this, &m_pipelineDesc, FrameGraph::QueueType::GRAPHICS);

	// common set
	builder.readWriteStorageBuffer(luminanceHistogramBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::LUMINANCE_HISTOGRAM_BINDING);
	builder.readWriteStorageBuffer(avgLuminanceBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::PERSISTENT_VALUES_BINDING);
}

void VEngine::VKLuminanceHistogramReduceAveragePass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry & registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], 0, nullptr);

	PushConsts pushConsts;
	pushConsts.precomputedTerm = 1.0f - exp(-m_timeDelta * 1.1f);
	pushConsts.lowerBound = m_width * m_height * 0.0f;
	pushConsts.upperBound = m_width * m_height * 1.0f;
	pushConsts.invScale = m_logMax - m_logMin;
	pushConsts.bias = -m_logMin * (1.0f / pushConsts.invScale);

	vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

	vkCmdDispatch(cmdBuf, 1, 1, 1);
}
