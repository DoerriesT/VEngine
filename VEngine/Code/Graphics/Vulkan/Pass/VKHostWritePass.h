#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKHostWritePass : FrameGraph::Pass
	{
	public:
		explicit VKHostWritePass(const char *name,
			const unsigned char *data,
			size_t srcOffset,
			size_t dstOffset,
			size_t srcItemSize,
			size_t dstItemSize,
			size_t srcItemStride,
			size_t count);
		void addToGraph(FrameGraph::Graph &graph, FrameGraph::BufferHandle &bufferHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline) override;

	private:
		FrameGraph::BufferHandle m_bufferHandle = 0;
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