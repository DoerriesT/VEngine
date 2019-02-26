#include "VKBlitPass.h"

VEngine::VKBlitPass::VKBlitPass(const char *name, uint32_t regionCount, const VkImageBlit *regions, VkFilter filter)
	:m_name(name),
	m_regionCount(regionCount),
	m_regions(regions),
	m_filter(filter)
{
}

void VEngine::VKBlitPass::addToGraph(FrameGraph::Graph &graph,
	FrameGraph::ImageHandle srcImage, 
	FrameGraph::ImageHandle dstImage)
{
	auto builder = graph.addPass(m_name, FrameGraph::PassType::BLIT, FrameGraph::QueueType::GRAPHICS, this);

	builder.readImageTransfer(srcImage);
	builder.writeImageTransfer(dstImage);

	m_srcImage = srcImage;
	m_dstImage = dstImage;
}

void VEngine::VKBlitPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry)
{
	vkCmdBlitImage(cmdBuf,
		registry.getImage(m_srcImage),
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		registry.getImage(m_dstImage),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		m_regionCount,
		m_regions,
		m_filter);
}
