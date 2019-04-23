#include "VKBlitPass.h"

void VEngine::VKBlitPass::addToGraph(FrameGraph::Graph &graph, const Data &data, const char *name)
{
	graph.addGenericPass(name, FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readImageTransfer(data.m_srcImage);
		builder.writeImageTransfer(data.m_dstImage);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		vkCmdBlitImage(cmdBuf,
			registry.getImage(data.m_srcImage),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			registry.getImage(data.m_dstImage),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			data.m_regionCount,
			data.m_regions,
			data.m_filter);
	});
}
