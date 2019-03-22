#include "VKVelocityInitializationPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"

VEngine::VKVelocityInitializationPass::VKVelocityInitializationPass(VKRenderResources *renderResources, 
	uint32_t width,
	uint32_t height, 
	size_t resourceIndex,
	const glm::mat4 &reprojectionMatrix)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_reprojectionMatrix(reprojectionMatrix)
{
	// create pipeline description
	strcpy_s(m_pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/fullscreenTriangle_vert.spv");
	strcpy_s(m_pipelineDesc.m_shaderStages.m_fragmentShaderPath, "Resources/Shaders/velocityInitialization_frag.spv");

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
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayouts[VELOCITY_INITIALIZATION_SET_INDEX];
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4) };
}

void VEngine::VKVelocityInitializationPass::addToGraph(FrameGraph::Graph &graph, FrameGraph::ImageHandle depthTextureHandle, FrameGraph::ImageHandle velocityTextureHandle)
{
	auto builder = graph.addGraphicsPass("Velocity Initialization", this, &m_pipelineDesc);

	builder.setDimensions(m_width, m_height);

	builder.readTexture(depthTextureHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, m_renderResources->m_pointSamplerClamp, m_renderResources->m_descriptorSets[m_resourceIndex][VELOCITY_INITIALIZATION_SET_INDEX], VelocityInitializationSetBindings::DEPTH_IMAGE_BINDING);

	builder.writeColorAttachment(velocityTextureHandle, 0);
}

void VEngine::VKVelocityInitializationPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry & registry, VkPipelineLayout layout, VkPipeline pipeline)
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

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][VELOCITY_INITIALIZATION_SET_INDEX], 0, nullptr);

	vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(m_reprojectionMatrix), &m_reprojectionMatrix);

	vkCmdDraw(cmdBuf, 3, 1, 0, 0);
}
