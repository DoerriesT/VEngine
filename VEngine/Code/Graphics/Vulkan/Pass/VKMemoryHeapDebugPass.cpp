#include "VKMemoryHeapDebugPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include <iostream>

struct PushConsts
{
	float scaleBias[4];
	float color[3];
};

VEngine::VKMemoryHeapDebugPass::VKMemoryHeapDebugPass(uint32_t width, uint32_t height,
	float offsetX, float offsetY, float scaleX, float scaleY)
	:m_width(width),
	m_height(height),
	m_offsetX(offsetX),
	m_offsetY(offsetY),
	m_scaleX(scaleX),
	m_scaleY(scaleY)
{
	// create pipeline description
	strcpy_s(m_pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/memoryHeapDebug_vert.spv");
	strcpy_s(m_pipelineDesc.m_shaderStages.m_fragmentShaderPath, "Resources/Shaders/memoryHeapDebug_frag.spv");

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

	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConsts) };
}

void VEngine::VKMemoryHeapDebugPass::addToGraph(FrameGraph::Graph &graph, FrameGraph::ImageHandle colorTextureHandle)
{
	auto builder = graph.addGraphicsPass("Memory Heap Debug Pass", this, &m_pipelineDesc, m_width, m_height);

	builder.writeColorAttachment(colorTextureHandle, 0);
}

void VEngine::VKMemoryHeapDebugPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline)
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

	auto debugInfo = g_context.m_allocator.getDebugInfo();
	const float maxBlockSize = static_cast<float>(g_context.m_allocator.getMaximumBlockSize());
	const float maxBlocks = 4.0f;
	const float blockSpacingMult = 1.05f;

	size_t memoryTypeBlockCount[VK_MAX_MEMORY_TYPES] = {};

	for (size_t i = 0; i < debugInfo.size(); ++i)
	{
		const auto &blockInfo = debugInfo[i];

		for (size_t j = 0; j < blockInfo.m_spans.size(); ++j)
		{
			const auto &span = blockInfo.m_spans[j];

			PushConsts pushConsts;

			switch (span.m_state)
			{
			case TLSFSpanDebugInfo::State::FREE:
				pushConsts.color[0] = 0.0f;
				pushConsts.color[1] = 1.0f;
				pushConsts.color[2] = 0.0f;
				break;
			case TLSFSpanDebugInfo::State::USED:
				pushConsts.color[0] = 0.0f;
				pushConsts.color[1] = 0.0f;
				pushConsts.color[2] = 1.0f;
				break;
			case TLSFSpanDebugInfo::State::WASTED:
				pushConsts.color[0] = 1.0f;
				pushConsts.color[1] = 0.0f;
				pushConsts.color[2] = 0.0f;
				break;
			default:
				assert(false);
				break;
			}

			pushConsts.scaleBias[0] = span.m_size / (maxBlockSize * maxBlocks);
			pushConsts.scaleBias[1] = 1.0f / float(VK_MAX_MEMORY_TYPES);
			pushConsts.scaleBias[2] = (memoryTypeBlockCount[blockInfo.m_memoryType] * blockSpacingMult + span.m_offset / maxBlockSize) / maxBlocks;
			pushConsts.scaleBias[3] = blockInfo.m_memoryType / float(VK_MAX_MEMORY_TYPES) * blockSpacingMult;

			vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);
			vkCmdDraw(cmdBuf, 6, 1, 0, 0);
		}

		++memoryTypeBlockCount[blockInfo.m_memoryType];
	}
}
