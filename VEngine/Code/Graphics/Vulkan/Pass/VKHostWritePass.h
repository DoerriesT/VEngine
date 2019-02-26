#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKHostWritePass : FrameGraph::Pass
	{
	public:
		explicit VKHostWritePass(const FrameGraph::BufferDescription &bufferDescription,
			const char *name,
			const unsigned char *data,
			size_t srcOffset,
			size_t dstOffset,
			size_t srcItemSize,
			size_t dstItemSize,
			size_t srcItemStride,
			size_t count);
		void addToGraph(FrameGraph::Graph &graph, FrameGraph::BufferHandle &bufferHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry) override;

	private:
		FrameGraph::BufferHandle m_bufferHandle = 0;
		FrameGraph::BufferDescription m_bufferDescription;
		const char *m_name;
		const unsigned char *m_data;
		size_t m_srcOffset;
		size_t m_dstOffset;
		size_t m_srcItemSize;
		size_t m_dstItemSize;
		size_t m_srcItemStride;
		size_t m_count;
	};
}