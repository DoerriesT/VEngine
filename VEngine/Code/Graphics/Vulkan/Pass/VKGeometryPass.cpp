#include "VKGeometryPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/DrawItem.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Mesh.h"

VEngine::VKGeometryPass::VKGeometryPass(
	VKRenderResources *renderResources,
	uint32_t width,
	uint32_t height,
	size_t resourceIndex,
	size_t drawItemCount,
	const DrawItem *drawItems,
	uint32_t drawItemBufferOffset,
	bool alphaMasked)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_drawItemCount(drawItemCount),
	m_drawItems(drawItems),
	m_drawItemBufferOffset(drawItemBufferOffset),
	m_alphaMasked(alphaMasked)
{
	// create pipeline description
	strcpy_s(m_pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/geometry_vert.spv");
	strcpy_s(m_pipelineDesc.m_shaderStages.m_fragmentShaderPath, m_alphaMasked ? "Resources/Shaders/geometry_alpha_mask_frag.spv" : "Resources/Shaders/geometry_frag.spv");

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
	m_pipelineDesc.m_rasterizationState.m_cullMode = m_alphaMasked ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
	m_pipelineDesc.m_rasterizationState.m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	m_pipelineDesc.m_rasterizationState.m_depthBiasEnable = false;
	m_pipelineDesc.m_rasterizationState.m_lineWidth = 1.0f;

	m_pipelineDesc.m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	m_pipelineDesc.m_multiSampleState.m_sampleShadingEnable = false;

	m_pipelineDesc.m_depthStencilState.m_depthTestEnable = true;
	m_pipelineDesc.m_depthStencilState.m_depthWriteEnable = true;
	m_pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	m_pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
	m_pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;

	VkPipelineColorBlendAttachmentState defaultBlendAttachment = {};
	defaultBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	defaultBlendAttachment.blendEnable = VK_FALSE;

	m_pipelineDesc.m_blendState.m_logicOpEnable = false;
	m_pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
	m_pipelineDesc.m_blendState.m_attachmentCount = 4;
	m_pipelineDesc.m_blendState.m_attachments[0] = defaultBlendAttachment;
	m_pipelineDesc.m_blendState.m_attachments[1] = defaultBlendAttachment;
	m_pipelineDesc.m_blendState.m_attachments[2] = defaultBlendAttachment;
	m_pipelineDesc.m_blendState.m_attachments[3] = defaultBlendAttachment;

	m_pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
	m_pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	m_pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

	m_pipelineDesc.m_layout.m_setLayoutCount = 3;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_perFrameDataDescriptorSetLayout;
	m_pipelineDesc.m_layout.m_setLayouts[1] = m_renderResources->m_perDrawDataDescriptorSetLayout;
	m_pipelineDesc.m_layout.m_setLayouts[2] = m_renderResources->m_textureDescriptorSetLayout;
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
	auto builder = graph.addGraphicsPass(m_alphaMasked ? "GBuffer Fill Alpha" : "GBuffer Fill", this, &m_pipelineDesc);

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

void VEngine::VKGeometryPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

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

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &m_renderResources->m_perFrameDataDescriptorSets[m_resourceIndex], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 2, 1, &m_renderResources->m_textureDescriptorSet, 0, nullptr);

	VkBuffer vertexBuffer = m_renderResources->m_vertexBuffer.getBuffer();
	size_t itemSize = VKUtility::align(sizeof(PerDrawData), g_context.m_properties.limits.minUniformBufferOffsetAlignment);

	for (size_t i = 0; i < m_drawItemCount; ++i)
	{
		const DrawItem &item = m_drawItems[i];
		vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer, &item.m_vertexOffset);

		uint32_t dynamicOffset = static_cast<uint32_t>(itemSize * (i + m_drawItemBufferOffset));
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &m_renderResources->m_perDrawDataDescriptorSets[m_resourceIndex], 1, &dynamicOffset);

		vkCmdDrawIndexed(cmdBuf, item.m_indexCount, 1, item.m_baseIndex, 0, 0);
	}
}
