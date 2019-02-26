#include "VKTilingPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"

VEngine::VKTilingPass::VKTilingPass(VkPipeline pipeline, 
	VkPipelineLayout pipelineLayout, 
	VKRenderResources *renderResources, 
	uint32_t width, 
	uint32_t height, 
	size_t resourceIndex,
	uint32_t pointLightCount)
	:m_pipeline(pipeline),
	m_pipelineLayout(pipelineLayout),
	m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_pointLightCount(pointLightCount)
{
}

void VEngine::VKTilingPass::addToGraph(FrameGraph::Graph &graph, 
	FrameGraph::BufferHandle perFrameDataBufferHandle, 
	FrameGraph::BufferHandle pointLightCullDataBufferHandle, 
	FrameGraph::BufferHandle &pointLightBitMaskBufferHandle)
{
	auto builder = graph.addPass("Tiling Pass", FrameGraph::PassType::COMPUTE, FrameGraph::QueueType::GRAPHICS, this);

	if (!pointLightBitMaskBufferHandle)
	{
		uint32_t w = m_width / 16 + ((m_width % 16 == 0) ? 0 : 1);
		uint32_t h = m_height / 16 + ((m_height % 16 == 0) ? 0 : 1);
		uint32_t tileCount = w * h;
		VkDeviceSize bufferSize = MAX_POINT_LIGHTS / 32 * sizeof(uint32_t) * tileCount;

		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Point Light Index Buffer";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = bufferSize;
		desc.m_hostVisible = false;

		pointLightBitMaskBufferHandle = builder.createBuffer(desc);
	}

	builder.readUniformBuffer(perFrameDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_perFrameDataDescriptorSets[m_resourceIndex], 0);

	builder.writeStorageBuffer(pointLightBitMaskBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_lightBitMaskDescriptorSets[m_resourceIndex], 0);

	builder.readStorageBuffer(pointLightCullDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_cullDataDescriptorSets[m_resourceIndex], 0);
}

void VEngine::VKTilingPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_renderResources->m_perFrameDataDescriptorSets[m_resourceIndex], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 1, 1, &m_renderResources->m_lightBitMaskDescriptorSets[m_resourceIndex], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 2, 1, &m_renderResources->m_cullDataDescriptorSets[m_resourceIndex], 0, nullptr);

	vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &m_pointLightCount);

	VKUtility::dispatchComputeHelper(cmdBuf, m_width, m_height, 1, 16, 16, 1);
}
