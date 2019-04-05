#include "VKRasterTilingPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/LightData.h"
#include <glm/gtx/transform.hpp>

struct PushConsts
{
	glm::mat4 transform;
	uint32_t index;
};

VEngine::VKRasterTilingPass::VKRasterTilingPass(VKRenderResources * renderResources, 
	uint32_t width, 
	uint32_t height, 
	size_t resourceIndex, 
	const LightData &lightData,
	const glm::mat4 &projection)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_lightData(lightData),
	m_pojection(projection)
{
	// create pipeline description
	strcpy_s(m_pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/rasterTiling_vert.spv");
	strcpy_s(m_pipelineDesc.m_shaderStages.m_fragmentShaderPath, "Resources/Shaders/rasterTiling_frag.spv");

	m_pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 1;
	m_pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[0] = { 0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX };

	m_pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 1;
	m_pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };

	m_pipelineDesc.m_inputAssemblyState.m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_pipelineDesc.m_inputAssemblyState.m_primitiveRestartEnable = false;

	m_pipelineDesc.m_viewportState.m_viewportCount = 1;
	m_pipelineDesc.m_viewportState.m_viewports[0] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
	m_pipelineDesc.m_viewportState.m_scissorCount = 1;
	m_pipelineDesc.m_viewportState.m_scissors[0] = { {0, 0}, {1, 1} };

	m_pipelineDesc.m_rasterizationState.m_depthClampEnable = false;
	m_pipelineDesc.m_rasterizationState.m_rasterizerDiscardEnable = false;
	m_pipelineDesc.m_rasterizationState.m_polygonMode = VK_POLYGON_MODE_FILL;
	m_pipelineDesc.m_rasterizationState.m_cullMode = VK_CULL_MODE_FRONT_BIT;
	m_pipelineDesc.m_rasterizationState.m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	m_pipelineDesc.m_rasterizationState.m_depthBiasEnable = false;
	m_pipelineDesc.m_rasterizationState.m_lineWidth = 1.0f;

	m_pipelineDesc.m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
	m_pipelineDesc.m_multiSampleState.m_sampleShadingEnable = false;
	m_pipelineDesc.m_multiSampleState.m_sampleMask = 0xFFFFFFFF;

	m_pipelineDesc.m_depthStencilState.m_depthTestEnable = false;
	m_pipelineDesc.m_depthStencilState.m_depthWriteEnable = false;
	m_pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	m_pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
	m_pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;


	m_pipelineDesc.m_blendState.m_logicOpEnable = false;
	m_pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
	m_pipelineDesc.m_blendState.m_attachmentCount = 0;

	m_pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
	m_pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	m_pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

	m_pipelineDesc.m_layout.m_setLayoutCount = 1;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayouts[COMMON_SET_INDEX];
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConsts) + sizeof(uint32_t) };
}

void VEngine::VKRasterTilingPass::addToGraph(FrameGraph::Graph &graph, FrameGraph::BufferHandle perFrameDataBufferHandle, FrameGraph::BufferHandle pointLightBitMaskBufferHandle)
{
	auto builder = graph.addGraphicsPass("Tiling Pass", this, &m_pipelineDesc, m_width / 2, m_height / 2);

	// common set
	builder.writeStorageBuffer(pointLightBitMaskBufferHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::POINT_LIGHT_BITMASK_BINDING);
}

void VEngine::VKRasterTilingPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry & registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_width / 2);
	viewport.height = static_cast<float>(m_height / 2);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = { m_width / 2, m_height / 2 };

	vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

	vkCmdBindIndexBuffer(cmdBuf, m_renderResources->m_lightProxyIndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT16);

	VkBuffer vertexBuffer = m_renderResources->m_lightProxyVertexBuffer.getBuffer();
	VkDeviceSize vertexOffset = 0;
	vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer, &vertexOffset);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], 0, nullptr);

	uint32_t alignedDomainSizeX = (m_width / RendererConsts::LIGHTING_TILE_SIZE + ((m_width % RendererConsts::LIGHTING_TILE_SIZE == 0) ? 0 : 1));

	vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4) + sizeof(uint32_t), sizeof(alignedDomainSizeX), &alignedDomainSizeX);

	for (size_t i = 0; i < m_lightData.m_pointLightData.size(); ++i)
	{
		const auto &item = m_lightData.m_pointLightData[i];

		PushConsts pushConsts;
		pushConsts.index = static_cast<uint32_t>(i);
		pushConsts.transform = glm::mat4(1.0);
		pushConsts.transform = m_pojection * glm::translate(glm::vec3(item.m_positionRadius)) * glm::scale(glm::vec3(item.m_positionRadius.w));

		vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

		vkCmdDrawIndexed(cmdBuf, 240, 1, 0, 0, 0);
	}
}
