#include "VKHostWritePass.h"
#include <cassert>

void VEngine::VKHostWritePass::addToGraph(FrameGraph::Graph &graph, const Data &data, const char *name)
{
	graph.addHostAccessPass(name,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.writeBufferFromHost(data.m_bufferHandle);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		const unsigned char *src = data.m_data + data.m_srcOffset;
		unsigned char *dst = (unsigned char *)registry.mapMemory(data.m_bufferHandle);
		{
			dst += data.m_dstOffset;
			for (size_t i = 0; i < data.m_count; ++i)
			{
				memcpy(dst, src, data.m_srcItemSize);
				src += data.m_srcItemStride;
				dst += data.m_dstItemSize;
			}
		}
		registry.unmapMemory(data.m_bufferHandle);
	});
}
