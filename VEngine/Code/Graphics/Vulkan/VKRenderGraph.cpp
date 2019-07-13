#include "VKRenderGraph.h"
#include <cassert>
#include <stack>
#include <algorithm>
#include "VKContext.h"
#include "Utility/Utility.h"

VEngine::VKRenderGraphPassBuilder::VKRenderGraphPassBuilder(VKRenderGraph &graph, uint32_t passIndex)
	:m_graph(graph),
	m_passIndex(passIndex)
{
}

void VEngine::VKRenderGraphPassBuilder::readDepthStencil(ImageViewHandle handle)
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
		VK_IMAGE_LAYOUT_MAX_ENUM);

	m_graph.m_framebufferInfo[m_graph.m_passDescriptions[m_passIndex].m_framebufferInfoIndex].m_depthStencilAttachment = handle;
}

void VEngine::VKRenderGraphPassBuilder::readInputAttachment(ImageViewHandle handle, uint32_t index)
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
		VK_IMAGE_LAYOUT_MAX_ENUM);

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

	++m_graph.m_passRefCounts[m_passIndex];
}

void VEngine::VKRenderGraphPassBuilder::readWriteStorageBuffer(BufferViewHandle handle, VkPipelineStageFlags stageFlags)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		stageFlags,
		VKRenderGraph::PassResourceUsage::READ_BIT | VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::STORAGE_BIT);

	++m_graph.m_passRefCounts[m_passIndex];
}

void VEngine::VKRenderGraphPassBuilder::writeDepthStencil(ImageViewHandle handle)
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
		VK_IMAGE_LAYOUT_MAX_ENUM);

	++m_graph.m_passRefCounts[m_passIndex];

	m_graph.m_framebufferInfo[m_graph.m_passDescriptions[m_passIndex].m_framebufferInfoIndex].m_depthStencilAttachment = handle;
}

void VEngine::VKRenderGraphPassBuilder::writeColorAttachment(ImageViewHandle handle, uint32_t index)
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
		VK_IMAGE_LAYOUT_MAX_ENUM);

	++m_graph.m_passRefCounts[m_passIndex];

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

	++m_graph.m_passRefCounts[m_passIndex];
}

void VEngine::VKRenderGraphPassBuilder::writeStorageBuffer(BufferViewHandle handle, VkPipelineStageFlags stageFlags)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		stageFlags,
		VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::STORAGE_BIT);

	++m_graph.m_passRefCounts[m_passIndex];
}

void VEngine::VKRenderGraphPassBuilder::writeBufferTransfer(BufferViewHandle handle)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::TRANSFER_BIT);

	++m_graph.m_passRefCounts[m_passIndex];
}

void VEngine::VKRenderGraphPassBuilder::writeImageTransfer(ImageViewHandle handle, VkImageLayout finalLayout)
{
	assert(handle);

	writeResourceUsages(ResourceViewHandle(handle),
		m_passIndex,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VKRenderGraph::PassResourceUsage::WRITE_BIT | VKRenderGraph::PassResourceUsage::TRANSFER_BIT,
		finalLayout);

	++m_graph.m_passRefCounts[m_passIndex];
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
	finalLayout = (finalLayout == VK_IMAGE_LAYOUT_MAX_ENUM) ? m_graph.getImageLayout(usageFlags) : finalLayout;
	VKRenderGraph::PassResourceUsage usage{ passIndex, stageMask, usageFlags, finalLayout };
	m_graph.forEachSubresource(handle, [&](uint32_t index)
	{
		m_graph.m_resourceUsages[index].push_back(usage);
		if (usage.m_usageFlags & VKRenderGraph::PassResourceUsage::READ_BIT)
		{
			++m_graph.m_subresourceRefCounts[index];
		}
	});
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

VkAccessFlags VEngine::VKRenderGraph::getAccessMask(uint32_t flags)
{
	// write and read/write
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::TRANSFER_BIT)
		return VK_ACCESS_TRANSFER_WRITE_BIT;
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::STORAGE_BIT)
		return (flags & PassResourceUsage::READ_BIT) ? VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_WRITE_BIT;
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::DEPTH_STENCIL_BIT)
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::ATTACHMENT_BIT)
		return (flags & PassResourceUsage::READ_BIT) ?
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT :
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// read
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::TRANSFER_BIT)
		return VK_ACCESS_TRANSFER_READ_BIT;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::STORAGE_BIT)
		return VK_ACCESS_SHADER_READ_BIT;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::DEPTH_STENCIL_BIT)
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::ATTACHMENT_BIT)
		return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::TEXTURE_BIT)
		return VK_ACCESS_SHADER_READ_BIT;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::UNIFORM_BIT)
		return VK_ACCESS_UNIFORM_READ_BIT;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::INDEX_BIT)
		return VK_ACCESS_INDEX_READ_BIT;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::VERTEX_BIT)
		return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::INDIRECT_BIT)
		return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

	assert(false);
	return VkAccessFlags();
}

VkImageLayout VEngine::VKRenderGraph::getImageLayout(uint32_t flags)
{
	// write and read/write
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::TRANSFER_BIT)
		return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::STORAGE_BIT)
		return VK_IMAGE_LAYOUT_GENERAL;
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::DEPTH_STENCIL_BIT)
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::ATTACHMENT_BIT)
		return (flags & PassResourceUsage::READ_BIT) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// read
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::TRANSFER_BIT)
		return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::STORAGE_BIT)
		return VK_IMAGE_LAYOUT_GENERAL;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::DEPTH_STENCIL_BIT)
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::ATTACHMENT_BIT)
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::TEXTURE_BIT)
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	assert(false);
	return VkImageLayout();
}

VkImageUsageFlags VEngine::VKRenderGraph::getImageUsageFlags(uint32_t flags)
{
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::TRANSFER_BIT)
		return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::TRANSFER_BIT)
		return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (flags & PassResourceUsage::STORAGE_BIT)
		return VK_IMAGE_USAGE_STORAGE_BIT;
	if (flags & PassResourceUsage::DEPTH_STENCIL_BIT)
		return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::ATTACHMENT_BIT)
		return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (flags & PassResourceUsage::READ_BIT ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0);
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::ATTACHMENT_BIT)
		return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::TEXTURE_BIT)
		return VK_IMAGE_USAGE_SAMPLED_BIT;

	assert(false);
	return VkImageUsageFlags();
}

VkBufferUsageFlags VEngine::VKRenderGraph::getBufferUsageFlags(uint32_t flags)
{
	if (flags & PassResourceUsage::READ_BIT & PassResourceUsage::TRANSFER_BIT)
		return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (flags & PassResourceUsage::WRITE_BIT & PassResourceUsage::TRANSFER_BIT)
		return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (flags & PassResourceUsage::UNIFORM_BIT)
		return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (flags & PassResourceUsage::STORAGE_BIT)
		return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (flags & PassResourceUsage::INDEX_BIT)
		return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (flags & PassResourceUsage::VERTEX_BIT)
		return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (flags & PassResourceUsage::INDIRECT_BIT)
		return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	assert(false);
	return VkBufferUsageFlags();
}

void VEngine::VKRenderGraph::cull(ResourceViewHandle finalResourceViewHandle)
{
	const size_t passCount = m_passDescriptions.size();
	const size_t subresourceCount = m_resourceUsages.size();

	// increment ref count on final resource handle
	forEachSubresource(finalResourceViewHandle, [&](uint32_t index) { ++m_subresourceRefCounts[index]; });

	std::stack<uint32_t> resourceStack;

	// fill stack with resources with refCount == 0
	for (uint32_t i = 0; i < subresourceCount; ++i)
	{
		if (m_subresourceRefCounts[i] == 0)
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
				&& m_passRefCounts[passIndex] != 0
				&& --m_passRefCounts[passIndex] == 0)
			{
				for (uint32_t i = 0; i < subresourceCount; ++i)
				{
					for (const auto &usage : m_resourceUsages[i])
					{
						// if read resource in culled pass, decrement resource refCount. if it falls to zero, add it to stack
						if ((usage.m_usageFlags & PassResourceUsage::READ_BIT)
							&& usage.m_passIndex == passIndex
							&& m_subresourceRefCounts[i] != 0
							&& --m_subresourceRefCounts[i] == 0)
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
			return m_passRefCounts[usage.m_passIndex] == 0;
		}), m_resourceUsages[i].end());
	}
}

void VEngine::VKRenderGraph::createResources()
{
	const size_t resourceCount = m_resourceDescriptions.size();

	// create images/buffers
	for (uint32_t resourceIndex = 0; resourceIndex < resourceCount; ++resourceIndex)
	{
		m_imageBuffers[resourceIndex].image = VK_NULL_HANDLE;
		const auto &resDesc = m_resourceDescriptions[resourceIndex];

		// find usage flags
		VkFlags usageFlags = 0;

		uint32_t refCount = 0;
		const uint32_t subresourcesEndIndex = (resDesc.m_image ? resDesc.m_levels * resDesc.m_layers : 1) + m_resourceUsageOffsets[resourceIndex];
		for (uint32_t i = m_resourceUsageOffsets[i]; i < subresourcesEndIndex; ++i)
		{
			refCount += m_subresourceRefCounts[i];
			for (const auto &usage : m_resourceUsages[i])
			{
				usageFlags |= resDesc.m_image ? getImageUsageFlags(usage.m_usageFlags) : getBufferUsageFlags(usage.m_usageFlags);
			}
		}

		if (refCount == 0)
		{
			continue;
		}

		// is resource image or buffer?
		if (resDesc.m_image)
		{
			// create image
			VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			imageCreateInfo.flags;
			imageCreateInfo.imageType = resDesc.m_imageType;
			imageCreateInfo.format = resDesc.m_format;
			imageCreateInfo.extent.width = resDesc.m_width;
			imageCreateInfo.extent.height = resDesc.m_height;
			imageCreateInfo.extent.depth = resDesc.m_depth;
			imageCreateInfo.mipLevels = resDesc.m_levels;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VkSampleCountFlagBits(resDesc.m_samples);
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.usage = usageFlags;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VKAllocationCreateInfo allocCreateInfo = {};
			allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

			if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_imageBuffers[resourceIndex].image, m_allocations[resourceIndex]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create image!", -1);
			}
		}
		else
		{
			// create buffer
			VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
			bufferCreateInfo.size = resDesc.m_size;
			bufferCreateInfo.usage = usageFlags;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VKAllocationCreateInfo allocCreateInfo = {};
			allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

			if (g_context.m_allocator.createBuffer(allocCreateInfo, bufferCreateInfo, m_imageBuffers[resourceIndex].buffer, m_allocations[resourceIndex]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create buffer!", -1);
			}

			//m_bufferOffsets[resourceIndex] = 0;
		}
	}

	const uint32_t viewCount = m_viewDescriptions.size();

	// create views
	for (uint32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex)
	{
		m_imageBufferViews[viewIndex].imageView = VK_NULL_HANDLE;

		const auto &viewDesc = m_viewDescriptions[viewIndex];

		if (viewDesc.m_image)
		{
			VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			viewInfo.image = m_imageBuffers[(size_t)viewDesc.m_resourceHandle - 1].image;
			viewInfo.viewType = viewDesc.m_viewType;
			viewInfo.format = viewDesc.m_format;
			viewInfo.components = viewDesc.m_components;
			viewInfo.subresourceRange = viewDesc.m_subresourceRange;

			if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_imageBufferViews[viewIndex].imageView) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create image view!", -1);
			}
		}
		else if (viewDesc.m_format != VK_FORMAT_MAX_ENUM)
		{
			VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
			viewInfo.buffer = m_imageBuffers[(size_t)viewDesc.m_resourceHandle - 1].buffer;
			viewInfo.format = viewDesc.m_format;
			viewInfo.offset = viewDesc.m_offset;
			viewInfo.range = viewDesc.m_range;

			if (vkCreateBufferView(g_context.m_device, &viewInfo, nullptr, &m_imageBufferViews[viewIndex].bufferView) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create buffer view!", -1);
			}
		}
	}
}

void VEngine::VKRenderGraph::createRenderPasses()
{
	const uint32_t passCount = m_passDescriptions.size();

	for (size_t passIndex = 0; passIndex < passCount; ++passIndex)
	{
		const auto &passDesc = m_passDescriptions[passIndex];

		// skip culled passes and non graphics passes
		if (!m_passRefCounts[passIndex] || passDesc.m_type != PassDescription::GRAPHICS)
		{
			continue;
		}

		VkSubpassDependency dependencies[2] =
		{
			{ VK_SUBPASS_EXTERNAL , 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0 },
			{ 0, VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT }
		};

		// fill VkAttachmentDescription and VkAttachmentReference arrays
		auto processAttachment = [&](ImageViewHandle handle, VkAttachmentReference &ref, VkAttachmentDescription &desc)
		{
			VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			// determine dependencies per subresource
			forEachSubresource(ResourceViewHandle(handle), [&](uint32_t index)
			{
				// find index of current pass usage in usages vector
				size_t usageIndex = 0;
				for (; usageIndex < m_resourceUsages[index].size() && m_resourceUsages[index][usageIndex].m_passIndex != passIndex; ++usageIndex);

				const PassResourceUsage *prev = usageIndex > 0 ? &m_resourceUsages[index][usageIndex - 1] : nullptr;
				const PassResourceUsage *cur = &m_resourceUsages[index][usageIndex];
				const PassResourceUsage *next = (usageIndex + 1) < m_resourceUsages[index].size() ? &m_resourceUsages[index][usageIndex + 1] : nullptr;

				auto layout = getImageLayout(cur->m_usageFlags);
				assert(layout == initialLayout || initialLayout == VK_IMAGE_LAYOUT_UNDEFINED);
				initialLayout = layout;

				// if layout does not stay the same or access is not read -> read, dependency is needed
				if (prev
					&& (prev->m_finalLayout != initialLayout)
					|| (prev->m_usageFlags & PassResourceUsage::WRITE_BIT)
					|| (cur->m_usageFlags & PassResourceUsage::WRITE_BIT))
				{
					dependencies[0].srcStageMask |= prev->m_stageMask;
					dependencies[0].dstStageMask |= cur->m_stageMask;
					dependencies[0].srcAccessMask |= getAccessMask(prev->m_usageFlags);
					dependencies[0].dstAccessMask |= getAccessMask(cur->m_usageFlags);
				}

				// if layout does not stay the same or access is not read -> read, dependency is needed
				if (next
					&& (cur->m_finalLayout != getImageLayout(next->m_usageFlags))
					|| (cur->m_usageFlags & PassResourceUsage::WRITE_BIT)
					|| (next->m_usageFlags & PassResourceUsage::WRITE_BIT))
				{
					dependencies[1].srcStageMask |= cur->m_stageMask;
					dependencies[1].dstStageMask |= next->m_stageMask;
					dependencies[1].srcAccessMask |= getAccessMask(cur->m_usageFlags);
					dependencies[1].dstAccessMask |= getAccessMask(next->m_usageFlags);
				}
			});

			assert(initialLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			ref.layout = initialLayout;

			const auto &viewDesc = m_viewDescriptions[(size_t)handle - 1];

			desc.flags = 0;
			desc.format = viewDesc.m_format;
			desc.samples = VkSampleCountFlagBits(m_resourceDescriptions[(size_t)viewDesc.m_resourceHandle - 1].m_samples);
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
			desc.initialLayout = initialLayout;
			desc.finalLayout = initialLayout;
		};

		uint32_t inputAttachmentCount = 0;
		uint32_t colorAttachmentCount = 0;
		VkAttachmentReference inputAttachmentRefs[VKRenderPassDescription::MAX_INPUT_ATTACHMENTS];
		VkAttachmentReference colorAttachmentRefs[VKRenderPassDescription::MAX_COLOR_ATTACHMENTS];
		VkAttachmentReference resolveAttachmentRefs[VKRenderPassDescription::MAX_COLOR_ATTACHMENTS];
		VkAttachmentReference depthAttachmentRef;
		uint32_t attachmentCount = 0;
		VkAttachmentDescription attachmentDescriptions[VKRenderPassDescription::MAX_ATTACHMENTS];
		VkImageView framebufferAttachments[VKRenderPassDescription::MAX_ATTACHMENTS];
		const auto &framebufferInfo = m_framebufferInfo[passDesc.m_framebufferInfoIndex];

		for (size_t i = 0; i < VKRenderPassDescription::MAX_ATTACHMENTS; ++i)
		{
			VkAttachmentReference *ref = nullptr;
			ImageViewHandle handle = nullptr;
			if (i < VKRenderPassDescription::MAX_INPUT_ATTACHMENTS)
			{
				handle = framebufferInfo.m_inputAttachments[i];
				ref = inputAttachmentRefs + i;
			}
			else if (i < VKRenderPassDescription::MAX_ATTACHMENTS - 1)
			{
				handle = framebufferInfo.m_colorAttachments[i - VKRenderPassDescription::MAX_INPUT_ATTACHMENTS];
				ref = colorAttachmentRefs + i - VKRenderPassDescription::MAX_INPUT_ATTACHMENTS;
			}
			else
			{
				handle = framebufferInfo.m_depthStencilAttachment;
				ref = &depthAttachmentRef;
			}
			if (!handle)
			{
				continue;
			}

			framebufferAttachments[attachmentCount] = m_imageBufferViews[(size_t)handle - 1].imageView;
			ref->attachment = attachmentCount;
			processAttachment(handle, *ref, attachmentDescriptions[attachmentCount]);

			attachmentCount++;
		}

		// create subpass
		VkSubpassDescription subpassDesc{};
		{
			subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDesc.inputAttachmentCount = inputAttachmentCount;
			subpassDesc.pInputAttachments = inputAttachmentRefs;
			subpassDesc.colorAttachmentCount = colorAttachmentCount;
			subpassDesc.pColorAttachments = colorAttachmentRefs;
			subpassDesc.pResolveAttachments = resolveAttachmentRefs;
			subpassDesc.pDepthStencilAttachment = (framebufferInfo.m_depthStencilAttachment ? &depthAttachmentRef : nullptr);
		}

		// create renderpass
		{
			VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = attachmentCount;
			renderPassInfo.pAttachments = attachmentDescriptions;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpassDesc;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies;

			if (vkCreateRenderPass(g_context.m_device, &renderPassInfo, nullptr, &m_renderpassFramebufferHandles[passIndex].first) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create render pass!", -1);
			}
		}

		// create framebuffer
		{
			VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferCreateInfo.renderPass = m_renderpassFramebufferHandles[passIndex].first;
			framebufferCreateInfo.attachmentCount = attachmentCount;
			framebufferCreateInfo.pAttachments = framebufferAttachments;
			framebufferCreateInfo.width = framebufferInfo.m_width;
			framebufferCreateInfo.height = framebufferInfo.m_height;
			framebufferCreateInfo.layers = 1;

			if (vkCreateFramebuffer(g_context.m_device, &framebufferCreateInfo, nullptr, &m_renderpassFramebufferHandles[passIndex].second) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create framebuffer!", -1);
			}
		}
	}
}

void VEngine::VKRenderGraph::createSynchronization()
{
	const size_t subresourceCount = m_resourceUsages.size();
	for (size_t subresourceIndex = 0; subresourceIndex < subresourceCount; ++subresourceIndex)
	{
		// skip culled subresources
		if (!m_subresourceRefCounts[subresourceIndex])
		{
			continue;
		}

		// find index of resource
		size_t resourceIndex = 0;
		while (resourceIndex < m_resourceUsageOffsets.size() && subresourceIndex < m_resourceUsageOffsets[resourceIndex++]);

		const auto &resDesc = m_resourceDescriptions[resourceIndex];

		const auto &resourceUsages = m_resourceUsages[subresourceIndex];
		QueueType prevQueueType = m_passDescriptions[resourceUsages[0].m_passIndex].m_queue;
		VkImageLayout prevLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		bool prevWrite = false;

		for (size_t i = 0; i < resourceUsages.size(); ++i)
		{
			const auto &usage = resourceUsages[i];

			const bool queueChange = prevQueueType != m_passDescriptions[usage.m_passIndex].m_queue;
			const bool layoutChange = resDesc.m_image && prevLayout != getImageLayout(usage.m_usageFlags);
			const bool executionDependency = usage.m_usageFlags & PassResourceUsage::WRITE_BIT;
			const bool memoryDependency = prevWrite;

			// we need a barrier
			if (layoutChange || executionDependency || memoryDependency)
			{
				VkAccessFlags srcAccessMask = 0;
				// OR all accessFlags of all previous read-only passes
				// read-only passes may overlap, so we need to wait for all of them
				if (executionDependency && !memoryDependency)
				{
					for (size_t j = i; j-- > 0;)
					{
						if (resourceUsages[j].m_usageFlags & PassResourceUsage::WRITE_BIT 
							|| m_passDescriptions[resourceUsages[j].m_passIndex].m_queue != m_passDescriptions[usage.m_passIndex].m_queue)
						{
							break;
						}
						srcAccessMask |= getAccessMask(resourceUsages[j].m_usageFlags);
					}
				}
				else
				{
					srcAccessMask = (i == 0) ? 0 : getAccessMask(resourceUsages[i - 1].m_usageFlags);
				}

				if (resDesc.m_image)
				{
					const uint32_t relativeIndex = subresourceIndex - m_resourceUsageOffsets[resourceIndex];
					const uint32_t layer = relativeIndex / resDesc.m_levels;
					const uint32_t level = relativeIndex % resDesc.m_levels;

					VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					imageBarrier.srcAccessMask = srcAccessMask;
					imageBarrier.dstAccessMask = getAccessMask(resourceUsages[i].m_usageFlags);
					imageBarrier.oldLayout = prevLayout;
					imageBarrier.newLayout = getImageLayout(usage.m_usageFlags);
					imageBarrier.srcQueueFamilyIndex = queueIndexFromQueueType(prevQueueType);
					imageBarrier.dstQueueFamilyIndex = queueIndexFromQueueType(m_passDescriptions[usage.m_passIndex].m_queue);
					imageBarrier.image = m_imageBuffers[resourceIndex].image;
					imageBarrier.subresourceRange = { getImageAspectFlags(usage.m_usageFlags), level, 1, layer, 1 };
				}
				else
				{
					VkBufferMemoryBarrier bufferBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
					bufferBarrier.srcAccessMask = srcAccessMask;
					bufferBarrier.dstAccessMask = getAccessMask(resourceUsages[i].m_usageFlags);
					bufferBarrier.srcQueueFamilyIndex = queueIndexFromQueueType(prevQueueType);
					bufferBarrier.dstQueueFamilyIndex = queueIndexFromQueueType(m_passDescriptions[usage.m_passIndex].m_queue);
					bufferBarrier.buffer = m_imageBuffers[resourceIndex].buffer;
					bufferBarrier.offset = 0;
					bufferBarrier.size = resDesc.m_size;
				}
			}

			// update prev values for next iteration
			prevQueueType = m_passDescriptions[usage.m_passIndex].m_queue;
			prevLayout = resDesc.m_image ? getImageLayout(usage.m_usageFlags) : VK_IMAGE_LAYOUT_UNDEFINED;
			prevWrite = usage.m_usageFlags & PassResourceUsage::WRITE_BIT;
		}
	}
}
