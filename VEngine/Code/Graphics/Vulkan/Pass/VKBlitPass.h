#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKBlitPass : FrameGraph::Pass
	{
	public:
		explicit VKBlitPass(const char *name, uint32_t regionCount, const VkImageBlit *regions, VkFilter filter);
		void addToGraph(FrameGraph::Graph &graph,
			FrameGraph::ImageHandle srcImage,
			FrameGraph::ImageHandle dstImage);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry) override;

	private:
		FrameGraph::ImageHandle m_srcImage = 0;
		FrameGraph::ImageHandle m_dstImage = 0;
		const char *m_name;
		uint32_t m_regionCount;
		const VkImageBlit *m_regions;
		VkFilter m_filter;
	};
}