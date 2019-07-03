#include "VKRenderGraph.h"
#include <cassert>
#include <stack>
#include <algorithm>

VEngine::VKRenderGraphPassBuilder::VKRenderGraphPassBuilder(VKRenderGraph &graph, uint32_t passIndex)
	:m_graph(graph),
	m_passIndex(passIndex)
{
}

void VEngine::VKRenderGraphPassBuilder::readDepthStencil(ImageViewHandle handle, VkImageLayout finalLayout)
{
	assert(handle);
	size_t viewIndex = (size_t)handle - 1;

	const auto &viewDesc = m_graph.m_viewDescriptions[viewIndex];
	assert((viewDesc.m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0);
	assert(viewDesc.m_subresourceRange.layerCount == 1 && viewDesc.m_subresourceRange.levelCount == 1);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::DEPTH_STENCIL_BIT,
		finalLayout);

	m_graph.m_framebufferInfo[m_graph.m_passDescriptions[m_passIndex].m_framebufferInfoIndex].m_depthStencilAttachment = handle;
}

void VEngine::VKRenderGraphPassBuilder::readInputAttachment(ImageViewHandle handle, uint32_t index, VkImageLayout finalLayout)
{
	assert(handle);
	size_t viewIndex = (size_t)handle - 1;

	const auto &viewDesc = m_graph.m_viewDescriptions[viewIndex];
	assert((viewDesc.m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0);
	assert(viewDesc.m_subresourceRange.layerCount == 1 && viewDesc.m_subresourceRange.levelCount == 1);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::ATTACHMENT_BIT,
		finalLayout);

	m_graph.m_framebufferInfo[m_graph.m_passDescriptions[m_passIndex].m_framebufferInfoIndex].m_inputAttachments[index] = handle;
}

void VEngine::VKRenderGraphPassBuilder::readTexture(ImageViewHandle handle, VkPipelineStageFlags stageFlags, VkImageLayout finalLayout)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		stageFlags,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::TEXTURE_BIT,
		finalLayout);
}

void VEngine::VKRenderGraphPassBuilder::readStorageImage(ImageViewHandle handle, VkPipelineStageFlags stageFlags, VkImageLayout finalLayout)
{
	assert(handle);
	size_t viewIndex = (size_t)handle - 1;

	const auto &viewDesc = m_graph.m_viewDescriptions[viewIndex];
	assert(viewDesc.m_subresourceRange.layerCount == 1 && viewDesc.m_subresourceRange.levelCount == 1);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		stageFlags,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::STORAGE_BIT,
		finalLayout);
}

void VEngine::VKRenderGraphPassBuilder::readStorageBuffer(BufferViewHandle handle, VkPipelineStageFlags stageFlags)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		stageFlags,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::STORAGE_BIT);
}

void VEngine::VKRenderGraphPassBuilder::readUniformBuffer(BufferViewHandle handle, VkPipelineStageFlags stageFlags)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		stageFlags,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::UNIFORM_BIT);
}

void VEngine::VKRenderGraphPassBuilder::readVertexBuffer(BufferViewHandle handle)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::VERTEX_BIT);
}

void VEngine::VKRenderGraphPassBuilder::readIndexBuffer(BufferViewHandle handle)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::INDEX_BIT);
}

void VEngine::VKRenderGraphPassBuilder::readIndirectBuffer(BufferViewHandle handle)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::INDIRECT_BIT);
}

void VEngine::VKRenderGraphPassBuilder::readImageTransfer(ImageViewHandle handle, VkImageLayout finalLayout)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::TRANSFER_BIT,
		finalLayout);
}

void VEngine::VKRenderGraphPassBuilder::readWriteStorageImage(ImageViewHandle handle, VkPipelineStageFlags stageFlags, VkImageLayout finalLayout)
{
	assert(handle);
	size_t viewIndex = (size_t)handle - 1;

	const auto &viewDesc = m_graph.m_viewDescriptions[viewIndex];
	assert(viewDesc.m_subresourceRange.layerCount == 1 && viewDesc.m_subresourceRange.levelCount == 1);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		stageFlags,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::STORAGE_BIT,
		finalLayout);
}

void VEngine::VKRenderGraphPassBuilder::readWriteStorageBuffer(BufferViewHandle handle, VkPipelineStageFlags stageFlags)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		stageFlags,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::STORAGE_BIT);
}

void VEngine::VKRenderGraphPassBuilder::writeDepthStencil(ImageViewHandle handle, VkImageLayout finalLayout)
{
	assert(handle);
	size_t viewIndex = (size_t)handle - 1;

	const auto &viewDesc = m_graph.m_viewDescriptions[viewIndex];
	assert((viewDesc.m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) != 0);
	assert(viewDesc.m_subresourceRange.layerCount == 1 && viewDesc.m_subresourceRange.levelCount == 1);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::DEPTH_STENCIL_BIT,
		finalLayout);

	m_graph.m_framebufferInfo[m_graph.m_passDescriptions[m_passIndex].m_framebufferInfoIndex].m_depthStencilAttachment = handle;
}

void VEngine::VKRenderGraphPassBuilder::writeColorAttachment(ImageViewHandle handle, uint32_t index, VkImageLayout finalLayout)
{
	assert(handle);
	size_t viewIndex = (size_t)handle - 1;

	const auto &viewDesc = m_graph.m_viewDescriptions[viewIndex];
	assert((viewDesc.m_subresourceRange.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT) != 0);
	assert(viewDesc.m_subresourceRange.layerCount == 1 && viewDesc.m_subresourceRange.levelCount == 1);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::ATTACHMENT_BIT,
		finalLayout);

	m_graph.m_framebufferInfo[m_graph.m_passDescriptions[m_passIndex].m_framebufferInfoIndex].m_colorAttachments[index] = handle;
}

void VEngine::VKRenderGraphPassBuilder::writeStorageImage(ImageViewHandle handle, VkPipelineStageFlags stageFlags, VkImageLayout finalLayout)
{
	assert(handle);
	size_t viewIndex = (size_t)handle - 1;

	const auto &viewDesc = m_graph.m_viewDescriptions[viewIndex];
	assert(viewDesc.m_subresourceRange.layerCount == 1 && viewDesc.m_subresourceRange.levelCount == 1);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		stageFlags,
		VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::STORAGE_BIT,
		finalLayout);
}

void VEngine::VKRenderGraphPassBuilder::writeStorageBuffer(BufferViewHandle handle, VkPipelineStageFlags stageFlags)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		stageFlags,
		VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::STORAGE_BIT);
}

void VEngine::VKRenderGraphPassBuilder::writeBufferTransfer(BufferViewHandle handle)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::TRANSFER_BIT);
}

void VEngine::VKRenderGraphPassBuilder::writeImageTransfer(ImageViewHandle handle, VkImageLayout finalLayout)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::TRANSFER_BIT,
		finalLayout);
}

VEngine::ImageViewHandle VEngine::VKRenderGraphPassBuilder::createImageView(const ImageViewDescription &viewDesc)
{
	return ImageViewHandle();
}

VEngine::ImageHandle VEngine::VKRenderGraphPassBuilder::createImage(const ImageDescription &imageDesc)
{
	return ImageHandle();
}

VEngine::BufferViewHandle VEngine::VKRenderGraphPassBuilder::createBufferView(const BufferViewDescription & viewDesc)
{
	return BufferViewHandle();
}

VEngine::BufferHandle VEngine::VKRenderGraphPassBuilder::createBuffer(const BufferDescription &bufferDesc)
{
	return BufferHandle();
}

void VEngine::VKRenderGraphPassBuilder::writeResourceUsages(ResourceViewHandle handle, uint32_t passIndex, VkPipelineStageFlags stageMask, uint32_t usageFlags, VkImageLayout finalLayout)
{
	VKRenderGraph::PassResourceUsage usage{ passIndex, stageMask, usageFlags, finalLayout };
	m_graph.forEachSubresource(handle, [&](uint32_t index) { m_graph.m_resourceUsages[index].push_back(usage); });
}

void VEngine::VKRenderGraph::forEachSubresource(ResourceViewHandle handle, std::function<void(uint32_t)> func)
{
	const size_t viewIndex = (size_t)handle - 1;
	const auto &viewDesc = m_viewDescriptions[viewIndex];

	const size_t resourceUsagesOffset = m_resourceUsageOffsets[(size_t)viewDesc.m_resourceHandle - 1];

	if (viewDesc.m_image)
	{
		const auto &resDesc = m_resourceDescriptions[(size_t)viewDesc.m_resourceHandle - 1];
		const uint32_t levels = resDesc.m_levels;
		const uint32_t layers = resDesc.m_layers;

		const auto &range = viewDesc.m_subresourceRange;

		for (size_t layer = range.baseArrayLayer; layer < range.layerCount + range.baseArrayLayer; ++layer)
		{
			for (size_t level = range.baseMipLevel; level < range.levelCount + range.baseMipLevel; ++level)
			{
				const uint32_t index = layer * levels + level;
				func(index + resourceUsagesOffset);
			}
		}
	}
	else
	{
		func(resourceUsagesOffset);
	}
}

void VEngine::VKRenderGraph::cull(ResourceViewHandle finalResourceViewHandle)
{
	const size_t passCount = m_passDescriptions.size();
	const size_t resourceCount = m_resourceDescriptions.size();
	const size_t resourceViewCount = m_viewDescriptions.size();
	const size_t subresourceCount = m_resourceUsages.size();
	const size_t allocationSize = passCount + resourceCount + resourceViewCount + subresourceCount;

	std::unique_ptr<uint32_t[]> counts = std::make_unique<uint32_t[]>(allocationSize);
	memset(counts.get(), 0, allocationSize * 4);

	uint32_t *passRefCounts = counts.get();
	uint32_t *resourceRefCounts = counts.get() + passCount;
	uint32_t *resourceViewRefCounts = counts.get() + passCount + resourceCount;
	uint32_t *subresourceRefCounts = counts.get() + passCount + resourceCount + resourceViewCount;

	// compute initial ref counts
	for (size_t i = 0; i < subresourceCount; ++i)
	{
		for (const auto &usage : m_resourceUsages[i])
		{
			// increment resource ref count for each reading pass
			if (usage.m_usageFlags & PassResourceUsage::READ_BIT)
			{
				++subresourceRefCounts[i];
			}

			// increment pass ref count for each written resource
			if (usage.m_usageFlags & PassResourceUsage::WRITE_BIT)
			{
				++passRefCounts[usage.m_passIndex];
			}
		}
	}

	// increment ref count on final resource handle
	forEachSubresource(finalResourceViewHandle, [&](uint32_t index) { ++subresourceRefCounts[index]; });

	std::stack<uint32_t> resourceStack;

	// fill stack with resources with refCount == 0
	for (uint32_t i = 0; i < subresourceCount; ++i)
	{
		if (subresourceRefCounts[i] == 0)
		{
			resourceStack.push(i);
		}
	}

	// cull passes/resources
	while (!resourceStack.empty())
	{
		uint32_t subresourceIndex = resourceStack.top();
		resourceStack.pop();

		// find writing passes
		for (const auto &usage : m_resourceUsages[subresourceIndex])
		{
			const uint32_t passIndex = usage.m_passIndex;

			// if writing pass, decrement refCount. if it falls to zero, decrement refcount of read resources
			if ((usage.m_usageFlags & PassResourceUsage::WRITE_BIT)
				&& passRefCounts[passIndex] != 0
				&& --passRefCounts[passIndex] == 0)
			{
				for (uint32_t i = 0; i < subresourceCount; ++i)
				{
					for (const auto &usage : m_resourceUsages[i])
					{
						// if read resource in culled pass, decrement resource refCount. if it falls to zero, add it to stack
						if ((usage.m_usageFlags & PassResourceUsage::READ_BIT)
							&& usage.m_passIndex == passIndex
							&& subresourceRefCounts[i] != 0
							&& --subresourceRefCounts[i] == 0)
						{
							resourceStack.push(i);
						}
					}
				}
			}
		}
	}

	// remove culled passes from resource usages
	for (uint32_t i = 0; i < subresourceCount; ++i)
	{
		m_resourceUsages[i].erase(std::remove_if(m_resourceUsages[i].begin(), m_resourceUsages[i].end(), [&](const auto &usage)
		{ 
			return passRefCounts[usage.m_passIndex] == 0; 
		}), m_resourceUsages[i].end());
	}

	// calculate resource ref counts
	for (uint32_t i = 0; i < resourceCount; ++i)
	{
		const auto &resDesc = m_resourceDescriptions[i];
		const uint32_t usageOffset = m_resourceUsageOffsets[i];

		if (resDesc.m_image)
		{
			const uint32_t levels = resDesc.m_levels;
			const uint32_t layers = resDesc.m_layers;

			for (size_t layer = 0; layer < layers; ++layer)
			{
				for (size_t level = 0; level < levels; ++level)
				{
					const uint32_t index = layer * levels + level + usageOffset;
					resourceRefCounts[i] += subresourceRefCounts[index];
				}
			}
		}
		else
		{
			resourceRefCounts[i] += subresourceRefCounts[usageOffset];
		}
	}

	// calculate view ref counts
	for (uint32_t i = 0; i < resourceViewCount; ++i)
	{
		forEachSubresource(ResourceViewHandle(i + 1), [&](uint32_t index) { resourceViewRefCounts[i] += subresourceRefCounts[index]; });
	}

	// set culled passes bits
	m_culledPasses.reserve(passCount);
	for (size_t i = 0; i < passCount; ++i)
	{
		m_culledPasses[i] = passRefCounts[i] != 0;
	}

	// set culled resources bits
	m_culledResources.reserve(resourceCount);
	for (size_t i = 0; i < resourceCount; ++i)
	{
		m_culledResources[i] = resourceRefCounts[i] != 0;
	}

	// set culled views bits
	m_culledViews.reserve(resourceViewCount);
	for (size_t i = 0; i < passCount; ++i)
	{
		m_culledViews[i] = resourceViewRefCounts[i] != 0;
	}
}
