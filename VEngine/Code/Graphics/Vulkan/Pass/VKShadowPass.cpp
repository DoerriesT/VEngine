#include "VKShadowPass.h"
#include "Graphics/LightData.h"
#include "Graphics/DrawItem.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Mesh.h"

VEngine::VKShadowPass::VKShadowPass(
	VKRenderResources *renderResources,
	uint32_t width,
	uint32_t height,
	size_t resourceIndex,
	size_t drawItemCount,
	const DrawItem *drawItems,
	uint32_t drawItemOffset,
	size_t shadowJobCount,
	const ShadowJob *shadowJobs)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_drawItemCount(drawItemCount),
	m_drawItems(drawItems),
	m_drawItemOffset(drawItemOffset),
	m_shadowJobCount(shadowJobCount),
	m_shadowJobs(shadowJobs)
{
	// create pipeline description
	strcpy_s(m_pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/shadows_vert.spv");

	m_pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 1;
	m_pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[0] = { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };

	m_pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 3;
	m_pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) };
	m_pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal), };
	m_pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord), };

	m_pipelineDesc.m_inputAssemblyState.m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_pipelineDesc.m_inputAssemblyState.m_primitiveRestartEnable = false;

	m_pipelineDesc.m_viewportState.m_viewportCount = 1;
	m_pipelineDesc.m_viewportState.m_viewports[0] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
	m_pipelineDesc.m_viewportState.m_scissorCount = 1;
	m_pipelineDesc.m_viewportState.m_scissors[0] = { {0, 0}, {1, 1} };

	m_pipelineDesc.m_rasterizationState.m_depthClampEnable = false;
	m_pipelineDesc.m_rasterizationState.m_rasterizerDiscardEnable = false;
	m_pipelineDesc.m_rasterizationState.m_polygonMode = VK_POLYGON_MODE_FILL;
	m_pipelineDesc.m_rasterizationState.m_cullMode = VK_CULL_MODE_BACK_BIT;
	m_pipelineDesc.m_rasterizationState.m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	m_pipelineDesc.m_rasterizationState.m_depthBiasEnable = false;
	m_pipelineDesc.m_rasterizationState.m_lineWidth = 1.0f;

	m_pipelineDesc.m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	m_pipelineDesc.m_multiSampleState.m_sampleShadingEnable = false;
	m_pipelineDesc.m_multiSampleState.m_sampleMask = 0xFFFFFFFF;

	m_pipelineDesc.m_depthStencilState.m_depthTestEnable = true;
	m_pipelineDesc.m_depthStencilState.m_depthWriteEnable = true;
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
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayout;
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16 + sizeof(uint32_t) };
}

void VEngine::VKShadowPass::addToGraph(FrameGraph::Graph &graph, 
	FrameGraph::BufferHandle perFrameDataBufferHandle,
	FrameGraph::BufferHandle perDrawDataBufferHandle,
	FrameGraph::ImageHandle shadowTextureHandle)
{
	auto builder = graph.addGraphicsPass("Shadow Pass", this, &m_pipelineDesc);
		
	builder.setDimensions(m_width, m_height);

	builder.readUniformBuffer(perFrameDataBufferHandle, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex], 0);
	builder.readStorageBuffer(perDrawDataBufferHandle, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex], 10);

	builder.writeDepthStencil(shadowTextureHandle);
}

void VEngine::VKShadowPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkViewport viewport = { 0, 0, m_width, m_height , 0.0f, 1.0f };
	vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

	// clear the relevant areas in the atlas
	{
		VkClearRect clearRects[8];

		VkClearAttachment clearAttachment = {};
		clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		clearAttachment.clearValue.depthStencil = { 1.0f, 0 };

		for (size_t i = 0; i < m_shadowJobCount; ++i)
		{
			const ShadowJob &job = m_shadowJobs[i];

			clearRects[i].rect = { { int32_t(job.m_offsetX), int32_t(job.m_offsetY) },{ job.m_size, job.m_size } };
			clearRects[i].baseArrayLayer = 0;
			clearRects[i].layerCount = 1;
		}

		vkCmdClearAttachments(cmdBuf, 1, &clearAttachment, m_shadowJobCount, clearRects);
	}

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex], 0, nullptr);

	vkCmdBindIndexBuffer(cmdBuf, m_renderResources->m_indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

	for (size_t i = 0; i < m_shadowJobCount; ++i)
	{
		const ShadowJob &job = m_shadowJobs[i];

		// set viewport and scissor
		VkViewport viewport = { job.m_offsetX, job.m_offsetY, job.m_size, job.m_size, 0.0f, 1.0f };
		VkRect2D scissor = { { job.m_offsetX, job.m_offsetY }, { job.m_size, job.m_size } };

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		const glm::mat4 &viewProjection = job.m_shadowViewProjectionMatrix;
		vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &viewProjection);

		VkBuffer vertexBuffer = m_renderResources->m_vertexBuffer.getBuffer();

		for (uint32_t i = 0; i < m_drawItemCount; ++i)
		{
			const DrawItem &item = m_drawItems[i];
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer, &item.m_vertexOffset);

			uint32_t offset = m_drawItemOffset + i;
			vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(offset), &offset);

			vkCmdDrawIndexed(cmdBuf, item.m_indexCount, 1, item.m_baseIndex, 0, 0);
		}
	}
}
