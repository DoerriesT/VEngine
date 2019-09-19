#include "LuminanceHistogramReadBackCopyPass.h"
#include "Graphics/RendererConsts.h"

void VEngine::LuminanceHistogramReadBackCopyPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_srcBuffer), ResourceState::READ_BUFFER_TRANSFER},
	};

	graph.addPass("Luminance Histogram Buffer Read Back Copy", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		VkBufferCopy region{ 0, 0, RendererConsts::LUMINANCE_HISTOGRAM_SIZE * sizeof(uint32_t) };
		vkCmdCopyBuffer(cmdBuf, registry.getBuffer(data.m_srcBuffer), data.m_dstBuffer, 1, &region);

		VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;

		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
	}, true);
}
