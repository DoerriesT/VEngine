#include "VKLuminanceHistogramDebugPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"

VEngine::VKLuminanceHistogramDebugPass::VKLuminanceHistogramDebugPass(VKRenderResources *renderResources,
	uint32_t width, 
	uint32_t height, 
	size_t resourceIndex,
	float offsetX, 
	float offsetY, 
	float scaleX, 
	float scaleY)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_offsetX(offsetX),
	m_offsetY(offsetY),
	m_scaleX(scaleX),
	m_scaleY(scaleY)
{
	// create pipeline description
	strcpy_s(m_pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/luminanceHistogramDebug_vert.spv");
	strcpy_s(m_pipelineDesc.m_shaderStages.m_fragmentShaderPath, "Resources/Shaders/luminanceHistogramDebug_frag.spv");

	m_pipelineDesc.m_inputAssemblyState.m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_pipelineDesc.m_inputAssemblyState.m_primitiveRestartEnable = false;

	m_pipelineDesc.m_viewportState.m_viewportCount = 1;
	m_pipelineDesc.m_viewportState.m_viewports[0] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
	m_pipelineDesc.m_viewportState.m_scissorCount = 1;
	m_pipelineDesc.m_viewportState.m_scissors[0] = { {0, 0}, {1, 1} };

	m_pipelineDesc.m_rasterizationState.m_depthClampEnable = false;
	m_pipelineDesc.m_rasterizationState.m_rasterizerDiscardEnable = false;
	m_pipelineDesc.m_rasterizationState.m_polygonMode = VK_POLYGON_MODE_FILL;
	m_pipelineDesc.m_rasterizationState.m_cullMode = VK_CULL_MODE_NONE;
	m_pipelineDesc.m_rasterizationState.m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	m_pipelineDesc.m_rasterizationState.m_depthBiasEnable = false;
	m_pipelineDesc.m_rasterizationState.m_lineWidth = 1.0f;

	m_pipelineDesc.m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	m_pipelineDesc.m_multiSampleState.m_sampleShadingEnable = false;
	m_pipelineDesc.m_multiSampleState.m_sampleMask = 0xFFFFFFFF;

	m_pipelineDesc.m_depthStencilState.m_depthTestEnable = false;
	m_pipelineDesc.m_depthStencilState.m_depthWriteEnable = false;
	m_pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	m_pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
	m_pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;

	VkPipelineColorBlendAttachmentState defaultBlendAttachment = {};
	defaultBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	defaultBlendAttachment.blendEnable = VK_FALSE;

	m_pipelineDesc.m_blendState.m_logicOpEnable = false;
	m_pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
	m_pipelineDesc.m_blendState.m_attachmentCount = 1;
	m_pipelineDesc.m_blendState.m_attachments[0] = defaultBlendAttachment;

	m_pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
	m_pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	m_pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

	m_pipelineDesc.m_layout.m_setLayoutCount = 1;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayouts[COMMON_SET_INDEX];
}

void VEngine::VKLuminanceHistogramDebugPass::addToGraph(FrameGraph::Graph &graph, 
	FrameGraph::BufferHandle perFrameDataBufferHandle, 
	FrameGraph::ImageHandle colorTextureHandle, 
	FrameGraph::BufferHandle luminanceHistogramBufferHandle)
{
	auto builder = graph.addGraphicsPass("Luminance Histogram Debug Pass", this, &m_pipelineDesc);

	builder.setDimensions(m_width, m_height);

	// common set
	builder.readUniformBuffer(perFrameDataBufferHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::PER_FRAME_DATA_BINDING);
	builder.readStorageBuffer(luminanceHistogramBufferHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::LUMINANCE_HISTOGRAM_BINDING);

	builder.writeColorAttachment(colorTextureHandle, 0);
}

void VEngine::VKLuminanceHistogramDebugPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry & registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkViewport viewport;
	viewport.x = m_width * m_offsetX;
	viewport.y = m_height * m_offsetY;
	viewport.width = static_cast<float>(m_width) * m_scaleX;
	viewport.height = static_cast<float>(m_height) * m_scaleY;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { static_cast<int32_t>(viewport.x), static_cast<int32_t>(viewport.y) };
	scissor.extent = { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };

	vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], 0, nullptr);

	vkCmdDraw(cmdBuf, 6, 1, 0, 0);
}
