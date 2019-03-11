#include "VKHostWritePass.h"
#include <cassert>

VEngine::VKHostWritePass::VKHostWritePass(const FrameGraph::BufferDescription &bufferDescription,
	const char *name,
	const unsigned char *data,
	size_t srcOffset,
	size_t dstOffset,
	size_t srcItemSize,
	size_t dstItemSize,
	size_t srcItemStride,
	size_t count)
	:m_bufferDescription(bufferDescription),
	m_name(name),
	m_data(data),
	m_srcOffset(srcOffset),
	m_dstOffset(dstOffset),
	m_srcItemSize(srcItemSize),
	m_dstItemSize(dstItemSize),
	m_srcItemStride(srcItemStride),
	m_count(count)
{
	assert(m_dstItemSize >= m_srcItemSize);
}

void VEngine::VKHostWritePass::addToGraph(FrameGraph::Graph &graph, FrameGraph::BufferHandle &bufferHandle)
{
	auto builder = graph.addHostAccessPass(m_name, this);
	
	if (!bufferHandle)
	{
		bufferHandle = builder.createBuffer(m_bufferDescription);
	}

	builder.writeBufferFromHost(bufferHandle);

	m_bufferHandle = bufferHandle;
}

void VEngine::VKHostWritePass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	assert(m_bufferDescription.m_size >= m_dstOffset + m_count * m_dstItemSize);

	const unsigned char *src = m_data + m_srcOffset;
	unsigned char *dst = (unsigned char *)registry.mapMemory(m_bufferHandle);
	{
		dst += m_dstOffset;
		for (size_t i = 0; i < m_count; ++i)
		{
			memcpy(dst, src, m_srcItemSize);
			src += m_srcItemStride;
			dst += m_dstItemSize;
		}
	}
	registry.unmapMemory(m_bufferHandle);
}
