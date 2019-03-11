#include "VKTilingPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"

VEngine::VKTilingPass::VKTilingPass(
	VKRenderResources *renderResources, 
	uint32_t width, 
	uint32_t height, 
	size_t resourceIndex,
	uint32_t pointLightCount)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_pointLightCount(pointLightCount)
{
	strcpy_s(m_pipelineDesc.m_computeShaderPath, "Resources/Shaders/tiling_comp.spv");

	m_pipelineDesc.m_layout.m_setLayoutCount = 3;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_perFrameDataDescriptorSetLayout;
	m_pipelineDesc.m_layout.m_setLayouts[1] = m_renderResources->m_lightBitMaskDescriptorSetLayout;
	m_pipelineDesc.m_layout.m_setLayouts[2] = m_renderResources->m_cullDataDescriptorSetLayout;
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) };
}

void VEngine::VKTilingPass::addToGraph(FrameGraph::Graph &graph, 
	FrameGraph::BufferHandle perFrameDataBufferHandle, 
	FrameGraph::BufferHandle pointLightCullDataBufferHandle, 
	FrameGraph::BufferHandle &pointLightBitMaskBufferHandle)
{
	auto builder = graph.addComputePass("Tiling Pass", this, &m_pipelineDesc, FrameGraph::QueueType::GRAPHICS);

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

void VEngine::VKTilingPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &m_renderResources->m_perFrameDataDescriptorSets[m_resourceIndex], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 1, 1, &m_renderResources->m_lightBitMaskDescriptorSets[m_resourceIndex], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 2, 1, &m_renderResources->m_cullDataDescriptorSets[m_resourceIndex], 0, nullptr);

	vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &m_pointLightCount);

	VKUtility::dispatchComputeHelper(cmdBuf, m_width, m_height, 1, 16, 16, 1);
}
