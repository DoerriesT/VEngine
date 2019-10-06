#include "ReadBackCopyPass.h"
#include "Graphics/RendererConsts.h"

void VEngine::ReadBackCopyPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_srcBuffer), ResourceState::READ_BUFFER_TRANSFER},
	};

	graph.addPass("Read Back Copy", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		vkCmdCopyBuffer(cmdBuf, registry.getBuffer(data.m_srcBuffer), data.m_dstBuffer, 1, &data.m_bufferCopy);

		VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}, true);
}
