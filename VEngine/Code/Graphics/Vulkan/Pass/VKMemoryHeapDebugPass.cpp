#include "VKMemoryHeapDebugPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include <iostream>

VEngine::VKMemoryHeapDebugPass::VKMemoryHeapDebugPass(VkPipeline pipeline, VkPipelineLayout pipelineLayout, uint32_t width, uint32_t height,
	float offsetX, float offsetY, float scaleX, float scaleY)
	:m_pipeline(pipeline),
	m_pipelineLayout(pipelineLayout),
	m_width(width),
	m_height(height),
	m_offsetX(offsetX),
	m_offsetY(offsetY),
	m_scaleX(scaleX),
	m_scaleY(scaleY)
{
}

void VEngine::VKMemoryHeapDebugPass::addToGraph(FrameGraph::Graph & graph, FrameGraph::ImageHandle colorTextureHandle)
{
	auto builder = graph.addPass("Memory Heap Debug Pass", FrameGraph::PassType::GRAPHICS, FrameGraph::QueueType::GRAPHICS, this);

	builder.setDimensions(m_width, m_height);
	builder.writeColorAttachment(colorTextureHandle, 0);
}

void VEngine::VKMemoryHeapDebugPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

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

			struct PushConsts
			{
				float scaleBias[4];
				float color[3];
			} pushConsts;

			switch (span.m_state)
			{
			case VKMemorySpanDebugInfo::State::FREE:
				pushConsts.color[0] = 0.0f;
				pushConsts.color[1] = 1.0f;
				pushConsts.color[2] = 0.0f;
				break;
			case VKMemorySpanDebugInfo::State::USED:
				pushConsts.color[0] = 0.0f;
				pushConsts.color[1] = 0.0f;
				pushConsts.color[2] = 1.0f;
				break;
			case VKMemorySpanDebugInfo::State::WASTED:
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

			vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);
			vkCmdDraw(cmdBuf, 6, 1, 0, 0);
		}

		++memoryTypeBlockCount[blockInfo.m_memoryType];
	}

	std::cout
		<< "free   " << g_context.m_allocator.getFreeMemorySize()
		<< "\nused   " << g_context.m_allocator.getUsedMemorySize()
		<< "\nwasted " << g_context.m_allocator.getWastedMemorySize()
		<< std::endl;
}
