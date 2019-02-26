#include "VKGeometryPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/DrawItem.h"
#include "Graphics/Vulkan/VKContext.h"

VEngine::VKGeometryPass::VKGeometryPass(VkPipeline pipeline, 
	VkPipelineLayout pipelineLayout, 
	VKRenderResources *renderResources, 
	uint32_t width, 
	uint32_t height, 
	size_t resourceIndex, 
	size_t drawItemCount,
	const DrawItem *drawItems,
	uint32_t drawItemBufferOffset, 
	bool alphaMasked)
	:m_pipeline(pipeline),
	m_pipelineLayout(pipelineLayout),
	m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_drawItemCount(drawItemCount),
	m_drawItems(drawItems),
	m_drawItemBufferOffset(drawItemBufferOffset),
	m_alphaMasked(alphaMasked)
{
}

void VEngine::VKGeometryPass::addToGraph(FrameGraph::Graph &graph, 
	FrameGraph::BufferHandle perFrameDataBufferHandle, 
	FrameGraph::BufferHandle perDrawDataBufferHandle, 
	FrameGraph::ImageHandle &depthTextureHandle, 
	FrameGraph::ImageHandle &albedoTextureHandle, 
	FrameGraph::ImageHandle &normalTextureHandle, 
	FrameGraph::ImageHandle &materialTextureHandle, 
	FrameGraph::ImageHandle &velocityTextureHandle)
{
	auto builder = graph.addPass(m_alphaMasked ? "GBuffer Fill Alpha" : "GBuffer Fill", FrameGraph::PassType::GRAPHICS, FrameGraph::QueueType::GRAPHICS, this);
		
	builder.setDimensions(m_width, m_height);

	if (!depthTextureHandle)
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "DepthTexture";
		desc.m_concurrent = false;
		desc.m_clear = true;
		desc.m_clearValue.m_imageClearValue.depthStencil.depth = 1.0f;
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_D32_SFLOAT;

		depthTextureHandle = builder.createImage(desc);
	}

	if (!albedoTextureHandle)
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "AlbedoTexture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R8G8B8A8_UNORM;

		albedoTextureHandle = builder.createImage(desc);
	}

	if (!normalTextureHandle)
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "NormalTexture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		normalTextureHandle = builder.createImage(desc);
	}

	if (!materialTextureHandle)
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "MaterialTexture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R8G8B8A8_UNORM;

		materialTextureHandle = builder.createImage(desc);
	}

	if (!velocityTextureHandle)
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "VelocityTexture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16_SFLOAT;

		velocityTextureHandle = builder.createImage(desc);
	}

	builder.readUniformBuffer(perFrameDataBufferHandle, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		m_renderResources->m_perFrameDataDescriptorSets[m_resourceIndex], 0);

	builder.readUniformBufferDynamic(perDrawDataBufferHandle, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VKUtility::align(sizeof(PerDrawData), g_context.m_properties.limits.minUniformBufferOffsetAlignment),
		m_renderResources->m_perDrawDataDescriptorSets[m_resourceIndex], 0);

	builder.writeDepthStencil(depthTextureHandle);
	builder.writeColorAttachment(albedoTextureHandle, 0);
	builder.writeColorAttachment(normalTextureHandle, 1);
	builder.writeColorAttachment(materialTextureHandle, 2);
	builder.writeColorAttachment(velocityTextureHandle, 3);
}

void VEngine::VKGeometryPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_width);
	viewport.height = static_cast<float>(m_height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = { m_width, m_height };

	vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

	vkCmdBindIndexBuffer(cmdBuf, m_renderResources->m_indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_renderResources->m_perFrameDataDescriptorSets[m_resourceIndex], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 2, 1, &m_renderResources->m_textureDescriptorSet, 0, nullptr);

	VkBuffer vertexBuffer = m_renderResources->m_vertexBuffer.getBuffer();
	size_t itemSize = VKUtility::align(sizeof(PerDrawData), g_context.m_properties.limits.minUniformBufferOffsetAlignment);

	for (size_t i = 0; i < m_drawItemCount; ++i)
	{
		const DrawItem &item = m_drawItems[i];
		vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer, &item.m_vertexOffset);

		uint32_t dynamicOffset = static_cast<uint32_t>(itemSize * (i + m_drawItemBufferOffset));
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1, &m_renderResources->m_perDrawDataDescriptorSets[m_resourceIndex], 1, &dynamicOffset);

		vkCmdDrawIndexed(cmdBuf, item.m_indexCount, 1, item.m_baseIndex, 0, 0);
	}
}
