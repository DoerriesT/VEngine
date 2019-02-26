#include "VKShadowPass.h"
#include "Graphics/LightData.h"
#include "Graphics/DrawItem.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKUtility.h"

VEngine::VKShadowPass::VKShadowPass(VkPipeline pipeline,
	VkPipelineLayout pipelineLayout,
	VKRenderResources *renderResources,
	uint32_t width,
	uint32_t height,
	size_t resourceIndex,
	size_t drawItemCount,
	const DrawItem *drawItems,
	uint32_t drawItemBufferOffset,
	size_t shadowJobCount,
	const ShadowJob *shadowJobs)
	:m_pipeline(pipeline),
	m_pipelineLayout(pipelineLayout),
	m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_drawItemCount(drawItemCount),
	m_drawItems(drawItems),
	m_drawItemBufferOffset(drawItemBufferOffset),
	m_shadowJobCount(shadowJobCount),
	m_shadowJobs(shadowJobs)
{
}

void VEngine::VKShadowPass::addToGraph(FrameGraph::Graph &graph, 
	FrameGraph::BufferHandle perFrameDataBufferHandle,
	FrameGraph::BufferHandle perDrawDataBufferHandle,
	FrameGraph::ImageHandle shadowTextureHandle)
{
	auto builder = graph.addPass("Shadow Pass", FrameGraph::PassType::GRAPHICS, FrameGraph::QueueType::GRAPHICS, this);

	builder.setDimensions(m_width, m_height);

	builder.readUniformBuffer(perFrameDataBufferHandle, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		m_renderResources->m_perFrameDataDescriptorSets[m_resourceIndex], 0);

	builder.readUniformBufferDynamic(perDrawDataBufferHandle, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VKUtility::align(sizeof(PerDrawData), g_context.m_properties.limits.minUniformBufferOffsetAlignment),
		m_renderResources->m_perDrawDataDescriptorSets[m_resourceIndex], 0);

	builder.writeDepthStencil(shadowTextureHandle);
}

void VEngine::VKShadowPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

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
		vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &viewProjection);

		VkBuffer vertexBuffer = m_renderResources->m_vertexBuffer.getBuffer();
		size_t itemSize = VKUtility::align(sizeof(PerDrawData), g_context.m_properties.limits.minUniformBufferOffsetAlignment);

		for (size_t i = 0; i < m_drawItemCount; ++i)
		{
			const DrawItem &item = m_drawItems[i];
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer, &item.m_vertexOffset);

			uint32_t dynamicOffset = static_cast<uint32_t>(itemSize * (i + m_drawItemBufferOffset));
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_renderResources->m_perDrawDataDescriptorSets[m_resourceIndex], 1, &dynamicOffset);

			vkCmdDrawIndexed(cmdBuf, item.m_indexCount, 1, item.m_baseIndex, 0, 0);
		}
	}
}
