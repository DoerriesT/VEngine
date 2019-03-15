#include "VKTextPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"

VEngine::VKTextPass::VKTextPass(VKRenderResources *renderResources, 
	size_t resourceIndex,
	uint32_t width, 
	uint32_t height, 
	uint32_t atlasTextureIndex, 
	size_t stringCount, 
	const String *strings)
	:m_renderResources(renderResources),
	m_resourceIndex(resourceIndex),
	m_width(width),
	m_height(height),
	m_atlasTextureIndex(atlasTextureIndex),
	m_stringCount(stringCount),
	m_strings(strings)
{
	// create pipeline description
	strcpy_s(m_pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/text_vert.spv");
	strcpy_s(m_pipelineDesc.m_shaderStages.m_fragmentShaderPath, "Resources/Shaders/text_frag.spv");

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

	m_pipelineDesc.m_depthStencilState.m_depthTestEnable = false;
	m_pipelineDesc.m_depthStencilState.m_depthWriteEnable = false;
	m_pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	m_pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
	m_pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	m_pipelineDesc.m_blendState.m_logicOpEnable = false;
	m_pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
	m_pipelineDesc.m_blendState.m_attachmentCount = 1;
	m_pipelineDesc.m_blendState.m_attachments[0] = colorBlendAttachment;

	m_pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
	m_pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
	m_pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

	m_pipelineDesc.m_layout.m_setLayoutCount = 1;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayout;
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 12 + sizeof(uint32_t) };
}

void VEngine::VKTextPass::addToGraph(FrameGraph::Graph &graph, FrameGraph::ImageHandle colorTextureHandle)
{
	auto builder = graph.addGraphicsPass("Text Pass", this, &m_pipelineDesc);

	builder.setDimensions(m_width, m_height);
	builder.writeColorAttachment(colorTextureHandle, 0);
}

void VEngine::VKTextPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline)
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
	scissor.offset = { static_cast<int32_t>(viewport.x), static_cast<int32_t>(viewport.y) };
	scissor.extent = { static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height) };

	vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex], 0, nullptr);

	const size_t symbolHeight = 48;
	const size_t symbolWidth = 24;
	const size_t atlasWidth = 1024;
	const size_t atlasHeight = 512;
	const size_t symbolsPerRow = atlasWidth / symbolWidth;
	const float invAtlasWidth = 1.0f / atlasWidth;
	const float invAtlasHeight = 1.0f / atlasHeight;
	const float invWidth = 1.0f / m_width;
	const float invHeight = 1.0f / m_height;
	const float texCoordSizeX = symbolWidth * invAtlasWidth;
	const float texCoordSizeY = symbolHeight * invAtlasHeight;
	const float scaleX = 0.4f;
	const float scaleY = 0.4f;
	const float charScaleX = symbolWidth * scaleX * invWidth;
	const float charScaleY = symbolHeight * scaleY * invHeight;

	for (size_t i = 0; i < m_stringCount; ++i)
	{
		const String &str = m_strings[i];
		const char *c = str.m_chars;

		size_t offsetX = str.m_positionX;
		size_t offsetY = str.m_positionY;

		while (*c)
		{
			if (*c == '\n')
			{
				offsetY += static_cast<size_t>(symbolHeight * scaleY);
				offsetX = str.m_positionX;
			}
			else
			{
				// we can skip rendering spaces
				if (*c != ' ')
				{
					struct PushConsts
					{
						float scaleBias[4];
						float color[4];
						float texCoordOffset[2];
						float texCoordSize[2];
						uint32_t atlasTextureIndex;
					} pushConsts;

					pushConsts.scaleBias[0] = charScaleX;
					pushConsts.scaleBias[1] = charScaleY;
					pushConsts.scaleBias[2] = offsetX * invWidth;
					pushConsts.scaleBias[3] = offsetY * invHeight;

					pushConsts.color[0] = 1.0f;
					pushConsts.color[1] = 1.0f;
					pushConsts.color[2] = 0.0f;
					pushConsts.color[3] = 1.0f;

					size_t atlasIndex = *c - 32; // atlas starts at symbol 32
					pushConsts.texCoordOffset[0] = ((atlasIndex % symbolsPerRow) * symbolWidth) * invAtlasWidth;
					pushConsts.texCoordOffset[1] = ((atlasIndex / symbolsPerRow) * symbolHeight) * invAtlasHeight;
					pushConsts.texCoordSize[0] = texCoordSizeX;
					pushConsts.texCoordSize[1] = texCoordSizeY;

					pushConsts.atlasTextureIndex = m_atlasTextureIndex;

					vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);
					vkCmdDraw(cmdBuf, 6, 1, 0, 0);
				}

				offsetX += static_cast<size_t>(symbolWidth * scaleX);

				if (offsetX + static_cast<size_t>(symbolWidth * scaleX) > m_width)
				{
					offsetY += static_cast<size_t>(symbolHeight * scaleY);
					offsetX = str.m_positionX;
				}
				
			}

			++c;
		}
	}
}
