#include "RenderGraph.h"
#include "Utility/Utility.h"
#include <cassert>
#include <memory>
#include <stack>
#include <algorithm>
#include "VKUtility.h"

void VEngine::RenderGraph::createPasses(ResourceViewHandle finalResourceHandle)
{
	const size_t passCount = m_passSubresources.size();
	const size_t subresourceCount = m_resourceUsages.size();
	const size_t viewCount = m_viewDescriptions.size();

	m_resourceLifetimes.resize(m_resourceDescriptions.size());
	m_culledResources.resize(m_resourceDescriptions.size());
	m_culledViews.resize(m_viewDescriptions.size());

	std::unique_ptr<uint16_t[]> refCounts = std::make_unique<uint16_t[]>(passCount + subresourceCount + viewCount);
	memset(refCounts.get(), 0, sizeof(uint16_t) * (passCount + subresourceCount + viewCount));
	uint16_t *passRefCounts = refCounts.get();
	uint16_t *subresourceRefCounts = refCounts.get() + passCount;
	uint16_t *viewRefCounts = refCounts.get() + passCount + subresourceCount;

	// cull passes and resources
	{
		uint16_t survivingPassCount = 0;

		// increment pass refcounts for each written resource
		for (size_t i = 0; i < passCount; ++i)
		{
			passRefCounts[i] += m_passSubresources[i].m_writeSubresourceCount;
			survivingPassCount += passRefCounts[i] ? 1 : 0;
		}

		// increment final resource ref count
		forEachSubresource(finalResourceHandle, [&](uint32_t index) { ++subresourceRefCounts[index]; });

		// increment subresource refcounts for each reading pass
		// and fill stack with resources where refCount == 0
		std::stack<uint16_t> resourceStack;
		for (size_t resourceIdx = 0; resourceIdx < m_resourceUsages.size(); ++resourceIdx)
		{
			for (size_t usageIdx = 0; usageIdx < m_resourceUsages[resourceIdx].size(); ++usageIdx)
			{
				const auto accessType = getRWAccessType(m_resourceUsages[resourceIdx][usageIdx].m_usageType);
				if (accessType == RWAccessType::READ || accessType == RWAccessType::READ_WRITE)
				{
					++subresourceRefCounts[resourceIdx];
				}
			}
			if (subresourceRefCounts[resourceIdx] == 0)
			{
				resourceStack.push(static_cast<uint16_t>(resourceIdx));
			}
		}

		// cull passes/resources
		while (!resourceStack.empty())
		{
			uint16_t subresourceIndex = resourceStack.top();
			resourceStack.pop();

			// find writing passes
			for (const auto &usage : m_resourceUsages[subresourceIndex])
			{
				const uint32_t passHandle = usage.m_passHandle;
				// if writing pass, decrement refCount. if it falls to zero, decrement refcount of read resources
				const auto accessType = getRWAccessType(usage.m_usageType);
				if ((accessType == RWAccessType::WRITE || accessType == RWAccessType::READ_WRITE) && passRefCounts[passHandle] != 0 && --passRefCounts[passHandle] == 0)
				{
					--survivingPassCount;
					const size_t start = m_passSubresources[passHandle].m_subresourcesOffset;
					const size_t end = m_passSubresources[passHandle].m_subresourcesOffset + m_passSubresources[passHandle].m_readSubresourceCount;
					for (size_t i = start; i < end; ++i)
					{
						auto &refCount = subresourceRefCounts[m_passSubresourceIndices[i]];
						if (refCount != 0 && --refCount == 0)
						{
							resourceStack.push(i);
						}
					}
				}
			}
		}

		// remove culled passes from resource usages
		for (auto &usages : m_resourceUsages)
		{
			usages.erase(std::remove_if(usages.begin(), usages.end(), [&](const auto &usage) {return passRefCounts[usage.m_passHandle] == 0; }), usages.end());
		}

		// determine resource lifetimes (up to this point pass handles are still ordered by the <-relation)
		std::vector<std::vector<uint16_t>> clearResources(passCount);
		for (size_t resourceIdx = 0; resourceIdx < m_resourceDescriptions.size(); ++resourceIdx)
		{
			std::pair<uint16_t, uint16_t> lifetime{ 0xFFFF, 0 };
			bool referenced = false;
			const uint32_t usageOffset = m_resourceUsageOffsets[resourceIdx];
			const uint32_t subresourceCount = m_resourceDescriptions[resourceIdx].m_subresourceCount;
			for (size_t subresourceIdx = usageOffset; subresourceIdx < subresourceCount + usageOffset; ++subresourceIdx)
			{
				if (!m_resourceUsages[resourceIdx].empty())
				{
					referenced = true;
					lifetime.first = std::min(lifetime.first, m_resourceUsages[resourceIdx].front().m_passHandle);
					lifetime.second = std::max(lifetime.second, m_resourceUsages[resourceIdx].back().m_passHandle);
				}
			}
			if (m_resourceDescriptions[resourceIdx].m_clear)
			{
				clearResources[lifetime.first].push_back(resourceIdx);
			}
			m_resourceLifetimes[resourceIdx] = lifetime;
			m_culledResources[resourceIdx] = referenced;
		}

		// fill pass handles of surviving passes and insert clear passes
		std::vector<uint16_t> handleToIndex(passCount);
		m_passIndices.reserve(survivingPassCount);
		for (uint16_t i = 0; i < passCount; ++i)
		{
			if (passRefCounts[i])
			{
				// insert clear pass
				if (!clearResources[i].empty())
				{
					const uint16_t clearPassHandle = static_cast<uint16_t>(m_passSubresources.size());

					// add and populate PassSubresources struct
					m_passSubresources.push_back({ static_cast<uint32_t>(m_passSubresources.size()) });
					auto &passSubresource = m_passSubresources.back();
					for (const auto resourceIdx : clearResources[i])
					{
						const uint32_t usageOffset = m_resourceUsageOffsets[resourceIdx];
						const uint32_t subresourceCount = m_resourceDescriptions[resourceIdx].m_subresourceCount;
						// insert clear pass resource usage and add subresources to pass subresource index list
						for (size_t subresourceIdx = usageOffset; subresourceIdx < subresourceCount + usageOffset; ++subresourceIdx)
						{
							m_resourceUsages[resourceIdx].insert(m_resourceUsages[resourceIdx].cbegin(),
								{
									clearPassHandle,
									m_resourceDescriptions[resourceIdx].m_image ? ResourceUsageType::WRITE_IMAGE_TRANSFER : ResourceUsageType::WRITE_BUFFER_TRANSFER,
									VK_PIPELINE_STAGE_TRANSFER_BIT,
									VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
								});
							m_passSubresourceIndices.push_back(subresourceIdx);
						}
						passSubresource.m_writeSubresourceCount += subresourceCount;
						m_resourceLifetimes[resourceIdx].first = clearPassHandle;
					}

					// create record func and set appropriate queue
					m_passRecordInfo.push_back({ [this, clrResources{std::move(clearResources[i])}](VkCommandBuffer cmdBuf, const Registry &registry)
					{
						for (const auto resourceIdx : clrResources)
						{
							const auto &resDesc = m_resourceDescriptions[resourceIdx];
							if (resDesc.m_image)
							{
								const VkImage image = registry.getImage(ImageHandle(resourceIdx + 1));
								const VkImageSubresourceRange subresourceRange{ VKUtility::imageAspectMaskFromFormat(resDesc.m_format), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
								if (VKUtility::isDepthFormat(resDesc.m_format))
								{
									vkCmdClearDepthStencilImage(cmdBuf, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &resDesc.m_clearValue.m_imageClearValue.depthStencil, 1, &subresourceRange);
								}
								else
								{
									vkCmdClearColorImage(cmdBuf, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &resDesc.m_clearValue.m_imageClearValue.color, 1, &subresourceRange);
								}
							}
							else
							{
								vkCmdFillBuffer(cmdBuf, registry.getBuffer(BufferHandle(resourceIdx + 1)), 0, resDesc.m_size, resDesc.m_clearValue.m_bufferClearValue);
							}
						}
					}, m_passRecordInfo[i].m_queue });
					handleToIndex.push_back(static_cast<uint16_t>(m_passIndices.size()));
					m_passIndices.push_back(clearPassHandle);
				}
				handleToIndex[i] = static_cast<uint16_t>(m_passIndices.size());
				m_passIndices.push_back(i);
			}
		}

		// correct resource lifetimes to use indices into m_passIndices
		for (size_t resourceIdx = 0; resourceIdx < m_resourceDescriptions.size(); ++resourceIdx)
		{
			if (m_culledResources[resourceIdx])
			{
				auto &lifetime = m_resourceLifetimes[resourceIdx];
				lifetime.first = handleToIndex[lifetime.first];
				lifetime.second = handleToIndex[lifetime.second];
			}
		}

		// determine image create flags and view ref counts
		for (uint16_t passIdx = 0; passIdx < passCount; ++passIdx)
		{
			if (passRefCounts[passIdx])
			{
				for (const auto viewIdx : m_viewUsages[passIdx])
				{
					++viewRefCounts[viewIdx];
					const auto &viewDesc = m_viewDescriptions[viewIdx];
					auto &resDesc = m_resourceDescriptions[(size_t)viewDesc.m_resourceHandle - 1];
					if (resDesc.m_image)
					{
						if (viewDesc.m_format != resDesc.m_format)
						{
							resDesc.m_imageFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
						}
						if (viewDesc.m_viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewDesc.m_viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
						{
							resDesc.m_imageFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
						}
						else if (viewDesc.m_viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
						{
							resDesc.m_imageFlags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
						}
					}
				}
			}
		}

		for (size_t viewIdx = 0; viewIdx < viewCount; ++viewIdx)
		{
			m_culledViews[viewIdx] = viewRefCounts[viewIdx] != 0;
		}
	}
}

void VEngine::RenderGraph::createResources()
{
	m_allocations.resize(m_resourceDescriptions.size());
	m_imageBuffers.resize(m_resourceDescriptions.size());
	m_imageBufferViews.resize(m_viewDescriptions.size());

	// create resources
	for (size_t resourceIndex = 0; resourceIndex < m_resourceDescriptions.size(); ++resourceIndex)
	{
		m_imageBuffers[resourceIndex].image = VK_NULL_HANDLE;

		if (!m_culledResources[resourceIndex])
		{
			continue;
		}

		const auto &resDesc = m_resourceDescriptions[resourceIndex];

		// find usage flags and refCount
		VkFlags usageFlags = 0;

		const uint32_t subresourcesEndIndex = resDesc.m_subresourceCount + m_resourceUsageOffsets[resourceIndex];
		for (uint32_t i = m_resourceUsageOffsets[resourceIndex]; i < subresourcesEndIndex; ++i)
		{
			for (const auto &usage : m_resourceUsages[i])
			{
				usageFlags |= getUsageFlags(usage.m_usageType);
			}
		}

		// is resource image or buffer?
		if (resDesc.m_image)
		{
			// create image
			VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			imageCreateInfo.flags = resDesc.m_imageFlags;
			imageCreateInfo.imageType = resDesc.m_imageType;
			imageCreateInfo.format = resDesc.m_format;
			imageCreateInfo.extent.width = resDesc.m_width;
			imageCreateInfo.extent.height = resDesc.m_height;
			imageCreateInfo.extent.depth = resDesc.m_depth;
			imageCreateInfo.mipLevels = resDesc.m_levels;
			imageCreateInfo.arrayLayers = resDesc.m_layers;
			imageCreateInfo.samples = VkSampleCountFlagBits(resDesc.m_samples);
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.usage = usageFlags;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VKAllocationCreateInfo allocCreateInfo = {};
			allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

			if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_imageBuffers[resourceIndex].image, m_allocations[resourceIndex]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create image!", EXIT_FAILURE);
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
				Utility::fatalExit("Failed to create buffer!", EXIT_FAILURE);
			}
		}
	}

	// create views
	for (uint32_t viewIndex = 0; viewIndex < m_viewDescriptions.size(); ++viewIndex)
	{
		m_imageBufferViews[viewIndex].imageView = VK_NULL_HANDLE;

		if (!m_culledViews[viewIndex])
		{
			continue;
		}

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
				Utility::fatalExit("Failed to create image view!", EXIT_FAILURE);
			}
		}
		else if (viewDesc.m_format != VK_FORMAT_UNDEFINED)
		{
			VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
			viewInfo.buffer = m_imageBuffers[(size_t)viewDesc.m_resourceHandle - 1].buffer;
			viewInfo.format = viewDesc.m_format;
			viewInfo.offset = viewDesc.m_offset;
			viewInfo.range = viewDesc.m_range;

			if (vkCreateBufferView(g_context.m_device, &viewInfo, nullptr, &m_imageBufferViews[viewIndex].bufferView) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create buffer view!", EXIT_FAILURE);
			}
		}
	}
}

// TODO: get image/buffer from subresource index
// TODO: account for external wait/signal semaphores
void VEngine::RenderGraph::createSynchronization()
{
	struct SemaphoreDependencyInfo
	{
		enum { EXTERNAL_PASS_HANDLE = 0xFFFFFFFF };
		VkPipelineStageFlags m_waitDstStageMask;
		uint16_t m_waitedOnPassHandle;
		uint16_t m_semaphoreIndex;
	};

	std::vector<std::pair<std::vector<SemaphoreDependencyInfo>, uint32_t>> semaphoreDependencies(m_passSubresources.size());

	for (const uint16_t passIndex : m_passIndices)
	{
		auto &passRecordInfo = m_passRecordInfo[passIndex];

		const auto &passSubresources = m_passSubresources[passIndex];
		const size_t start = passSubresources.m_subresourcesOffset;
		const size_t end = passSubresources.m_subresourcesOffset + passSubresources.m_readSubresourceCount + passSubresources.m_writeSubresourceCount;
		for (size_t subresourceIndex = start; subresourceIndex < end; ++subresourceIndex)
		{
			const size_t usageCount = m_resourceUsages[subresourceIndex].size();

			// find index of current pass usage in usages vector
			size_t usageIndex = 0;
			for (; usageIndex < usageCount && m_resourceUsages[subresourceIndex][usageIndex].m_passHandle != passIndex; ++usageIndex);

			const auto &curUsage = m_resourceUsages[subresourceIndex][usageIndex];
			const auto &prevUsage = usageIndex == 0 ? curUsage : m_resourceUsages[subresourceIndex][usageIndex - 1];
			const auto &nextUsage = (usageIndex == (usageCount - 1)) ? curUsage : m_resourceUsages[subresourceIndex][usageIndex + 1];

			const bool imageResource = isImageResource(curUsage.m_usageType);
			const VkImageLayout previousLayout = usageIndex == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : prevUsage.m_finalLayout;
			const VkImageLayout currentLayout = getImageLayout(curUsage.m_usageType);
			const VkImageLayout nextLayout = (usageIndex == (usageCount - 1)) ? currentLayout : getImageLayout(nextUsage.m_usageType);
			const bool previousWriteAccess = usageIndex == 0 ? false : getRWAccessType(prevUsage.m_usageType) != RWAccessType::READ;
			const bool currentWriteAccess = getRWAccessType(curUsage.m_usageType) != RWAccessType::READ;
			const VkQueue currentQueue = passRecordInfo.m_queue;
			const VkQueue previousQueue = usageIndex == 0 ? currentQueue : m_passRecordInfo[prevUsage.m_passHandle].m_queue;
			const VkQueue nextQueue = (usageIndex == (usageCount - 1)) ? currentQueue : m_passRecordInfo[nextUsage.m_passHandle].m_queue;

			const bool executionDependency = currentWriteAccess;
			const bool memoryDependency = previousWriteAccess;

			VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			imageBarrier.srcAccessMask = previousWriteAccess ? getAccessMask(prevUsage.m_usageType) : 0;
			imageBarrier.dstAccessMask = getAccessMask(curUsage.m_usageType);
			imageBarrier.oldLayout = previousLayout;
			imageBarrier.newLayout = currentLayout;
			imageBarrier.srcQueueFamilyIndex = getQueueIndex(previousQueue);
			imageBarrier.dstQueueFamilyIndex = getQueueIndex(currentQueue);
			imageBarrier.image; // TODO
			imageBarrier.subresourceRange; // TODO;

			VkBufferMemoryBarrier bufferBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
			bufferBarrier.srcAccessMask = imageBarrier.srcAccessMask;
			bufferBarrier.dstAccessMask = imageBarrier.dstAccessMask;
			bufferBarrier.srcQueueFamilyIndex = imageBarrier.srcQueueFamilyIndex;
			bufferBarrier.dstQueueFamilyIndex = imageBarrier.dstQueueFamilyIndex;
			bufferBarrier.buffer; // TODO
			bufferBarrier.offset; // TODO;
			bufferBarrier.size; // TODO;

			bool addWaitBarrier = false;

			// acquire resource + schedule wait on semaphore
			if (previousQueue != currentQueue)
			{
				addWaitBarrier = true;

				// try to merge dependency with existing dependencies
				bool foundExistingDependency = false;
				for (auto &dependency : semaphoreDependencies[passIndex].first)
				{
					if (dependency.m_waitedOnPassHandle == prevUsage.m_passHandle)
					{
						dependency.m_waitDstStageMask |= getStageMask(curUsage.m_usageType, curUsage.m_stageMask);
						foundExistingDependency = true;
						break;
					}
				}
				if (!foundExistingDependency)
				{
					const uint16_t semaphoreIndex = semaphoreDependencies[prevUsage.m_passHandle].second++;
					semaphoreDependencies[passIndex].first.push_back({ getStageMask(curUsage.m_usageType, curUsage.m_stageMask), prevUsage.m_passHandle, semaphoreIndex });
				}
			}
			else
			{
				if (imageResource && previousLayout != currentLayout) // image layout transition
				{
					addWaitBarrier = true;
				}
				else if (memoryDependency)
				{
					passRecordInfo.m_memoryBarrierSrcAccessMask |= getAccessMask(prevUsage.m_usageType);
					passRecordInfo.m_memoryBarrierDstAccessMask |= getAccessMask(curUsage.m_usageType);
				}

				// schedule wait on event
				if (executionDependency || memoryDependency)
				{
					passRecordInfo.m_eventSrcStages |= getStageMask(prevUsage.m_usageType, prevUsage.m_stageMask);
					passRecordInfo.m_waitStages |= getStageMask(curUsage.m_usageType, curUsage.m_stageMask);
					passRecordInfo.m_waitEvents.push_back(VkEvent(prevUsage.m_passHandle));

					m_passRecordInfo[prevUsage.m_passHandle].m_endStages |= getStageMask(prevUsage.m_usageType, prevUsage.m_stageMask);
				}
			}

			if (addWaitBarrier)
			{
				if (imageResource)
				{
					passRecordInfo.m_imageBarriers.push_back(imageBarrier);
					// make sure all wait barriers come before all release barriers
					if (passRecordInfo.m_releaseImageBarrierCount)
					{
						std::swap(passRecordInfo.m_imageBarriers.back(), passRecordInfo.m_imageBarriers[passRecordInfo.m_waitImageBarrierCount]);
					}
					++passRecordInfo.m_waitImageBarrierCount;
				}
				else
				{
					passRecordInfo.m_bufferBarriers.push_back(bufferBarrier);
					// make sure all wait barriers come before all release barriers
					if (passRecordInfo.m_releaseBufferBarrierCount)
					{
						std::swap(passRecordInfo.m_bufferBarriers.back(), passRecordInfo.m_bufferBarriers[passRecordInfo.m_waitBufferBarrierCount]);
					}
					++passRecordInfo.m_waitBufferBarrierCount;
				}
			}

			// release resource
			if (currentQueue != nextQueue)
			{
				imageBarrier.srcAccessMask = currentWriteAccess ? getAccessMask(curUsage.m_usageType) : 0;
				imageBarrier.dstAccessMask = getAccessMask(nextUsage.m_usageType);
				imageBarrier.oldLayout = currentLayout;
				imageBarrier.newLayout = nextLayout;
				imageBarrier.srcQueueFamilyIndex = getQueueIndex(currentQueue);
				imageBarrier.dstQueueFamilyIndex = getQueueIndex(nextQueue);

				bufferBarrier.srcAccessMask = imageBarrier.srcAccessMask;
				bufferBarrier.dstAccessMask = imageBarrier.dstAccessMask;
				bufferBarrier.srcQueueFamilyIndex = imageBarrier.srcQueueFamilyIndex;
				bufferBarrier.dstQueueFamilyIndex = imageBarrier.dstQueueFamilyIndex;

				if (imageResource)
				{
					passRecordInfo.m_imageBarriers.push_back(imageBarrier);
					++passRecordInfo.m_releaseImageBarrierCount;
				}
				else
				{
					passRecordInfo.m_bufferBarriers.push_back(bufferBarrier);
					++passRecordInfo.m_releaseBufferBarrierCount;
				}

				passRecordInfo.m_releaseStages |= getStageMask(curUsage.m_usageType, curUsage.m_stageMask);
				semaphoreDependencies[passIndex].second = true;
			}
		}
	}

	// create events, semaphores and batches
	std::vector<uint16_t> semaphoreOffsets(m_passSubresources.size());
	VkQueue prevQueue = 0;
	bool startNewBatch = true;
	for (uint16_t i = 0; i < m_passIndices.size(); ++i)
	{
		const uint16_t passIndex = m_passIndices[i];
		const auto &semaphoreDependency = semaphoreDependencies[passIndex];
		VkQueue curQueue = m_passRecordInfo[passIndex].m_queue;

		// if this pass needs to wait, we need a need batch
		// if the previous pass needs to signal, startNewBatch is already true
		// if the queue type changed, we also need to start a new batch
		startNewBatch = startNewBatch || !semaphoreDependency.first.empty() || prevQueue != curQueue;
		if (startNewBatch)
		{
			startNewBatch = false;
			m_batches.push_back({});
			auto &batch = m_batches.back();
			batch.m_passIndexOffset = i;
			batch.m_queue = curQueue;
		}

		prevQueue = curQueue;

		// some other passes needs to wait on this one -> allocate semaphores, end batch after this pass
		if (semaphoreDependency.second)
		{
			startNewBatch = true;
			semaphoreOffsets[passIndex] = m_semaphores.size();
			auto &signalSemaphores = m_batches.back().m_signalSemaphores;
			signalSemaphores.reserve(semaphoreDependency.second + signalSemaphores.size());
			for (uint16_t semaphoreIndex = 0; semaphoreIndex < semaphoreDependency.second; ++semaphoreIndex)
			{
				VkSemaphore semaphore = g_context.m_syncPrimitivePool.acquireSemaphore();
				signalSemaphores.push_back(semaphore);
				m_semaphores.push_back(semaphore);
			}
		}

		// this pass needs to wait on other passes -> fill in wait semaphores and waitDstMasks
		if (!semaphoreDependency.first.empty())
		{
			auto &batch = m_batches.back();
			auto &waitSemaphores = batch.m_waitSemaphores;
			auto &waitDstMasks = batch.m_waitDstStageMasks;
			assert(waitSemaphores.size() == waitDstMasks.size());
			waitSemaphores.reserve(semaphoreDependency.first.size() + waitSemaphores.size());
			waitDstMasks.reserve(semaphoreDependency.first.size() + waitDstMasks.size());
			for (const auto &waitDependency : semaphoreDependency.first)
			{
				waitSemaphores.push_back(m_semaphores[waitDependency.m_semaphoreIndex + semaphoreOffsets[waitDependency.m_waitedOnPassHandle]]);
				waitDstMasks.push_back(waitDependency.m_waitDstStageMask);
			}
		}

		++m_batches.back().m_passIndexCount;

		// events
		{
			auto &passRecordInfo = m_passRecordInfo[passIndex];

			// some other pass wants to wait on this one -> allocate event
			if (passRecordInfo.m_endStages != 0)
			{
				VkEvent event = g_context.m_syncPrimitivePool.acquireEvent();
				passRecordInfo.m_endEvent = event;
			}

			// replace all wait events (currently holding an index) with the actual events
			for (auto &waitEvent : passRecordInfo.m_waitEvents)
			{
				VkEvent e = m_passRecordInfo[(size_t)waitEvent].m_endEvent;
				assert(e != VK_NULL_HANDLE);
				waitEvent = e;
			}
		}
	}
}

void VEngine::RenderGraph::record()
{
	for (const auto &batch : m_batches)
	{
		for (uint16_t i = 0; i < batch.m_passIndexCount; ++i)
		{
			const uint16_t passHandle = m_passIndices[i + batch.m_passIndexOffset];
			const auto &pass = m_passRecordInfo[passHandle];

			// get command buffer
			VkCommandBuffer cmdBuf = VK_NULL_HANDLE;

			// start recording
			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(cmdBuf, &beginInfo);

			// wait on events
			if (!pass.m_waitEvents.empty())
			{
				VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
				memoryBarrier.srcAccessMask = pass.m_memoryBarrierSrcAccessMask;
				memoryBarrier.dstAccessMask = pass.m_memoryBarrierDstAccessMask;

				vkCmdWaitEvents(cmdBuf, static_cast<uint32_t>(pass.m_waitEvents.size()), pass.m_waitEvents.data(),
					pass.m_eventSrcStages, pass.m_waitStages, 1, &memoryBarrier,
					pass.m_waitBufferBarrierCount, pass.m_bufferBarriers.data(),
					pass.m_waitImageBarrierCount, pass.m_imageBarriers.data());
			}

			// pipeline barrier
			if (pass.m_waitEvents.empty() && (pass.m_waitBufferBarrierCount || pass.m_waitImageBarrierCount))
			{
				vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pass.m_waitStages, 0, 0, nullptr,
					pass.m_waitBufferBarrierCount, pass.m_bufferBarriers.data(),
					pass.m_waitImageBarrierCount, pass.m_imageBarriers.data());
			}

			// record commands
			pass.m_recordFunc(cmdBuf, Registry());

			// release resources
			if (pass.m_releaseBufferBarrierCount || pass.m_releaseImageBarrierCount)
			{
				vkCmdPipelineBarrier(cmdBuf, pass.m_releaseStages, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr,
					pass.m_releaseBufferBarrierCount, pass.m_bufferBarriers.data() + pass.m_waitBufferBarrierCount,
					pass.m_releaseImageBarrierCount, pass.m_imageBarriers.data() + pass.m_waitImageBarrierCount);
			}

			// signal end event
			if (pass.m_endEvent != VK_NULL_HANDLE)
			{
				vkCmdSetEvent(cmdBuf, pass.m_endEvent, pass.m_endStages);
			}

			// end recording
			vkEndCommandBuffer(cmdBuf);
		}
	}

	// submit all batches
	for (const auto &batch : m_batches)
	{
		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = static_cast<uint32_t>(batch.m_waitSemaphores.size());
		submitInfo.pWaitSemaphores = batch.m_waitSemaphores.data();
		submitInfo.pWaitDstStageMask = batch.m_waitDstStageMasks.data();
		//submitInfo.commandBufferCount = static_cast<uint32_t>(batch.m_commandBuffers.size());
		submitInfo.signalSemaphoreCount = static_cast<uint32_t>(batch.m_signalSemaphores.size());
		submitInfo.pSignalSemaphores = batch.m_signalSemaphores.data();

		if (vkQueueSubmit(batch.m_queue, 1, &submitInfo, batch.m_fence) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to submit batch to queue!", EXIT_FAILURE);
		}
	}
}

uint32_t VEngine::RenderGraph::getQueueIndex(VkQueue queue)
{
	for (size_t i = 0; i < 3; ++i)
	{
		if (m_queues[i] == queue)
		{
			return m_queueFamilyIndices[i];
		}
	}
	assert(false);
	return uint32_t();
}

VkQueue VEngine::RenderGraph::getQueue(QueueType queueType)
{
	return m_queues[static_cast<size_t>(queueType)];
}

bool VEngine::RenderGraph::isImageResource(ResourceUsageType usageType)
{
	switch (usageType)
	{
	case ResourceUsageType::READ_DEPTH_STENCIL:
	case ResourceUsageType::READ_ATTACHMENT:
	case ResourceUsageType::READ_TEXTURE:
	case ResourceUsageType::READ_STORAGE_IMAGE:
	case ResourceUsageType::READ_IMAGE_TRANSFER:
	case ResourceUsageType::READ_WRITE_STORAGE_IMAGE:
	case ResourceUsageType::READ_WRITE_ATTACHMENT:
	case ResourceUsageType::WRITE_DEPTH_STENCIL:
	case ResourceUsageType::WRITE_ATTACHMENT:
	case ResourceUsageType::WRITE_STORAGE_IMAGE:
	case ResourceUsageType::WRITE_IMAGE_TRANSFER:
		return true;
	case ResourceUsageType::READ_STORAGE_BUFFER:
	case ResourceUsageType::READ_UNIFORM_BUFFER:
	case ResourceUsageType::READ_VERTEX_BUFFER:
	case ResourceUsageType::READ_INDEX_BUFFER:
	case ResourceUsageType::READ_INDIRECT_BUFFER:
	case ResourceUsageType::READ_BUFFER_TRANSFER:
	case ResourceUsageType::READ_WRITE_STORAGE_BUFFER:
	case ResourceUsageType::WRITE_BUFFER_TRANSFER:
		return false;
	default:
		assert(false);
	}
	return false;
}

VkPipelineStageFlags VEngine::RenderGraph::getStageMask(ResourceUsageType usageType, VkPipelineStageFlags flags)
{
	switch (usageType)
	{
	case ResourceUsageType::WRITE_DEPTH_STENCIL:
	case ResourceUsageType::READ_DEPTH_STENCIL:
		return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	case ResourceUsageType::READ_ATTACHMENT:
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	case ResourceUsageType::READ_WRITE_STORAGE_IMAGE:
	case ResourceUsageType::READ_WRITE_STORAGE_BUFFER:
	case ResourceUsageType::READ_TEXTURE:
	case ResourceUsageType::READ_STORAGE_IMAGE:
	case ResourceUsageType::READ_STORAGE_BUFFER:
	case ResourceUsageType::READ_UNIFORM_BUFFER:
	case ResourceUsageType::WRITE_STORAGE_IMAGE:
		return flags;
	case ResourceUsageType::READ_VERTEX_BUFFER:
	case ResourceUsageType::READ_INDEX_BUFFER:
		return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	case ResourceUsageType::READ_INDIRECT_BUFFER:
		return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
	case ResourceUsageType::READ_BUFFER_TRANSFER:
	case ResourceUsageType::READ_IMAGE_TRANSFER:
	case ResourceUsageType::WRITE_BUFFER_TRANSFER:
	case ResourceUsageType::WRITE_IMAGE_TRANSFER:
		return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case ResourceUsageType::READ_WRITE_ATTACHMENT:
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	case ResourceUsageType::WRITE_ATTACHMENT:
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	default:
		assert(false);
	}
	return VkPipelineStageFlags();
}

VkAccessFlags VEngine::RenderGraph::getAccessMask(ResourceUsageType usageType)
{
	switch (usageType)
	{
	case ResourceUsageType::READ_DEPTH_STENCIL:
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	case ResourceUsageType::READ_ATTACHMENT:
		return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	case ResourceUsageType::READ_TEXTURE:
	case ResourceUsageType::READ_STORAGE_IMAGE:
	case ResourceUsageType::READ_STORAGE_BUFFER:
		return VK_ACCESS_SHADER_READ_BIT;
	case ResourceUsageType::READ_UNIFORM_BUFFER:
		return VK_ACCESS_UNIFORM_READ_BIT;
	case ResourceUsageType::READ_VERTEX_BUFFER:
		return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	case ResourceUsageType::READ_INDEX_BUFFER:
		return VK_ACCESS_INDEX_READ_BIT;
	case ResourceUsageType::READ_INDIRECT_BUFFER:
		return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	case ResourceUsageType::READ_BUFFER_TRANSFER:
	case ResourceUsageType::READ_IMAGE_TRANSFER:
		return VK_ACCESS_TRANSFER_READ_BIT;
	case ResourceUsageType::READ_WRITE_STORAGE_IMAGE:
	case ResourceUsageType::READ_WRITE_STORAGE_BUFFER:
		return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	case ResourceUsageType::READ_WRITE_ATTACHMENT:
		return VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case ResourceUsageType::WRITE_DEPTH_STENCIL:
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case ResourceUsageType::WRITE_ATTACHMENT:
		return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case ResourceUsageType::WRITE_STORAGE_IMAGE:
		return VK_ACCESS_SHADER_WRITE_BIT;
	case ResourceUsageType::WRITE_BUFFER_TRANSFER:
	case ResourceUsageType::WRITE_IMAGE_TRANSFER:
		return VK_ACCESS_TRANSFER_WRITE_BIT;
	default:
		assert(false);
	}
	return VkAccessFlags();
}

VkImageLayout VEngine::RenderGraph::getImageLayout(ResourceUsageType usageType)
{
	switch (usageType)
	{
	case ResourceUsageType::READ_DEPTH_STENCIL:
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	case ResourceUsageType::READ_ATTACHMENT:
	case ResourceUsageType::READ_TEXTURE:
	case ResourceUsageType::READ_STORAGE_IMAGE:
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	case ResourceUsageType::READ_IMAGE_TRANSFER:
		return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	case ResourceUsageType::READ_WRITE_STORAGE_IMAGE:
	case ResourceUsageType::READ_WRITE_ATTACHMENT:
		return VK_IMAGE_LAYOUT_GENERAL;
	case ResourceUsageType::WRITE_DEPTH_STENCIL:
		return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	case ResourceUsageType::WRITE_ATTACHMENT:
		return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	case ResourceUsageType::WRITE_STORAGE_IMAGE:
		return VK_IMAGE_LAYOUT_GENERAL;
	case ResourceUsageType::WRITE_IMAGE_TRANSFER:
		return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	default:
		assert(false);
	}
	return VkImageLayout();
}

VEngine::RenderGraph::RWAccessType VEngine::RenderGraph::getRWAccessType(ResourceUsageType usageType)
{
	switch (usageType)
	{
	case ResourceUsageType::READ_DEPTH_STENCIL:
	case ResourceUsageType::READ_ATTACHMENT:
	case ResourceUsageType::READ_TEXTURE:
	case ResourceUsageType::READ_STORAGE_IMAGE:
	case ResourceUsageType::READ_STORAGE_BUFFER:
	case ResourceUsageType::READ_UNIFORM_BUFFER:
	case ResourceUsageType::READ_VERTEX_BUFFER:
	case ResourceUsageType::READ_INDEX_BUFFER:
	case ResourceUsageType::READ_INDIRECT_BUFFER:
	case ResourceUsageType::READ_BUFFER_TRANSFER:
	case ResourceUsageType::READ_IMAGE_TRANSFER:
		return RWAccessType::READ;
	case ResourceUsageType::READ_WRITE_STORAGE_IMAGE:
	case ResourceUsageType::READ_WRITE_STORAGE_BUFFER:
	case ResourceUsageType::READ_WRITE_ATTACHMENT:
		return RWAccessType::READ_WRITE;
	case ResourceUsageType::WRITE_DEPTH_STENCIL:
	case ResourceUsageType::WRITE_ATTACHMENT:
	case ResourceUsageType::WRITE_STORAGE_IMAGE:
	case ResourceUsageType::WRITE_BUFFER_TRANSFER:
	case ResourceUsageType::WRITE_IMAGE_TRANSFER:
		return RWAccessType::WRITE;
	default:
		assert(false);
	}
	return RWAccessType();
}

VkFlags VEngine::RenderGraph::getUsageFlags(ResourceUsageType usageType)
{
	VkBufferUsageFlags;
	switch (usageType)
	{
	case ResourceUsageType::WRITE_DEPTH_STENCIL:
	case ResourceUsageType::READ_DEPTH_STENCIL:
		return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	case ResourceUsageType::READ_ATTACHMENT:
		return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	case ResourceUsageType::READ_TEXTURE:
		return VK_IMAGE_USAGE_SAMPLED_BIT;
	case ResourceUsageType::READ_STORAGE_BUFFER:
		return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	case ResourceUsageType::READ_UNIFORM_BUFFER:
		return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	case ResourceUsageType::READ_VERTEX_BUFFER:
		return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	case ResourceUsageType::READ_INDEX_BUFFER:
		return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	case ResourceUsageType::READ_INDIRECT_BUFFER:
		return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	case ResourceUsageType::READ_BUFFER_TRANSFER:
		return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	case ResourceUsageType::READ_IMAGE_TRANSFER:
		return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	case ResourceUsageType::READ_STORAGE_IMAGE:
	case ResourceUsageType::WRITE_STORAGE_IMAGE:
	case ResourceUsageType::READ_WRITE_STORAGE_IMAGE:
		return VK_IMAGE_USAGE_STORAGE_BIT;
	case ResourceUsageType::READ_WRITE_STORAGE_BUFFER:
	case ResourceUsageType::READ_WRITE_ATTACHMENT:
		return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	case ResourceUsageType::WRITE_ATTACHMENT:
		return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	case ResourceUsageType::WRITE_BUFFER_TRANSFER:
		return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	case ResourceUsageType::WRITE_IMAGE_TRANSFER:
		return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	default:
		assert(false);
	}
	return VkFlags();
}

void VEngine::RenderGraph::addPass(const char *name, QueueType queueType, uint32_t passResourceUsageCount, const ResourceUsageDescription *passResourceUsages, const RecordFunc &recordFunc)
{
	const uint16_t passIndex = static_cast<uint16_t>(m_passSubresources.size());

	m_passRecordInfo.push_back({});
	auto &recordInfo = m_passRecordInfo.back();
	recordInfo.m_queue = getQueue(queueType);
	recordInfo.m_recordFunc = recordFunc;

	m_passSubresources.push_back({ static_cast<uint32_t>(m_passSubresourceIndices.size()) });
	auto &passSubresources = m_passSubresources.back();

	m_viewUsages.push_back({});

	// determine number of read- and write resources and update resource usages
	for (uint32_t i = 0; i < passResourceUsageCount; ++i)
	{
		const auto &usage = passResourceUsages[i];
		const auto &viewDesc = m_viewDescriptions[(size_t)usage.m_handle - 1];
		const uint16_t resourceIndex = (uint16_t)viewDesc.m_resourceHandle - 1;
		const auto &resDesc = m_resourceDescriptions[resourceIndex];
		const auto &range = viewDesc.m_subresourceRange;
		const uint16_t subresourceCount = resDesc.m_image ? range.levelCount * range.layerCount : 1;

		m_viewUsages[passIndex].push_back((uint16_t)usage.m_handle - 1);

		auto rwAccessType = getRWAccessType(usage.m_usageType);
		passSubresources.m_readSubresourceCount += rwAccessType != RWAccessType::WRITE ? subresourceCount : 0;
		passSubresources.m_writeSubresourceCount += rwAccessType != RWAccessType::READ ? subresourceCount : 0;

		const ResourceUsage resUsage{ passIndex, usage.m_usageType, usage.m_stageMask, resDesc.m_image && usage.m_finalLayout == VK_IMAGE_LAYOUT_UNDEFINED ? getImageLayout(usage.m_usageType) : usage.m_finalLayout };
		forEachSubresource(usage.m_handle, [&](uint32_t index) { m_resourceUsages[index].push_back(resUsage); });
	}

	m_passSubresources.reserve(m_passSubresources.size() + passSubresources.m_readSubresourceCount + passSubresources.m_writeSubresourceCount);

	// add read resource indices to list and update resource usages
	for (uint32_t i = 0; i < passResourceUsageCount; ++i)
	{
		const auto &usage = passResourceUsages[i];
		if (getRWAccessType(usage.m_usageType) != RWAccessType::WRITE)
		{
			forEachSubresource(usage.m_handle, [&](uint32_t index) { m_passSubresourceIndices.push_back(static_cast<uint16_t>(index)); });
		}
	}
	// add write resources
	for (uint32_t i = 0; i < passResourceUsageCount; ++i)
	{
		const auto &usage = passResourceUsages[i];
		if (getRWAccessType(usage.m_usageType) != RWAccessType::READ)
		{
			forEachSubresource(usage.m_handle, [&](uint32_t index) { m_passSubresourceIndices.push_back(static_cast<uint16_t>(index)); });
		}
	}
}

VEngine::ImageViewHandle VEngine::RenderGraph::createImageView(const ImageViewDescription &viewDesc)
{
	ResourceViewDescription desc{};
	desc.m_name = viewDesc.m_name;
	desc.m_resourceHandle = ResourceHandle(viewDesc.m_imageHandle);
	desc.m_viewType = viewDesc.m_viewType;
	desc.m_format = viewDesc.m_format;
	desc.m_components = viewDesc.m_components;
	desc.m_subresourceRange = viewDesc.m_subresourceRange;
	desc.m_image = true;

	m_viewDescriptions.push_back(desc);

	return ImageViewHandle(m_viewDescriptions.size());
}

VEngine::BufferViewHandle VEngine::RenderGraph::createBufferView(const BufferViewDescription &viewDesc)
{
	ResourceViewDescription desc{};
	desc.m_name = viewDesc.m_name;
	desc.m_resourceHandle = ResourceHandle(viewDesc.m_bufferHandle);
	desc.m_format = viewDesc.m_format;
	desc.m_offset = viewDesc.m_offset;
	desc.m_range = viewDesc.m_range;

	m_viewDescriptions.push_back(desc);

	return BufferViewHandle(m_viewDescriptions.size());
}

VEngine::ImageHandle VEngine::RenderGraph::createImage(const ImageDescription &imageDesc)
{
	ResourceDescription resDesc{};
	resDesc.m_name = imageDesc.m_name;
	resDesc.m_clear = imageDesc.m_clear;
	resDesc.m_clearValue = imageDesc.m_clearValue;
	resDesc.m_width = imageDesc.m_width;
	resDesc.m_height = imageDesc.m_height;
	resDesc.m_depth = imageDesc.m_depth;
	resDesc.m_layers = imageDesc.m_layers;
	resDesc.m_levels = imageDesc.m_levels;
	resDesc.m_samples = imageDesc.m_samples;
	resDesc.m_imageType = imageDesc.m_imageType;
	resDesc.m_format = imageDesc.m_format;
	resDesc.m_subresourceCount = resDesc.m_layers * resDesc.m_levels;
	resDesc.m_image = true;

	m_resourceDescriptions.push_back(resDesc);
	m_resourceUsageOffsets.push_back(static_cast<uint32_t>(m_resourceUsages.size()));
	for (size_t i = 0; i < resDesc.m_subresourceCount; ++i)
	{
		m_resourceUsages.push_back({});
	}

	return ImageHandle(m_resourceDescriptions.size());
}

VEngine::BufferHandle VEngine::RenderGraph::createBuffer(const BufferDescription &bufferDesc)
{
	ResourceDescription resDesc{};
	resDesc.m_name = bufferDesc.m_name;
	resDesc.m_clear = bufferDesc.m_clear;
	resDesc.m_clearValue = bufferDesc.m_clearValue;
	resDesc.m_size = bufferDesc.m_size;
	resDesc.m_subresourceCount = 1;

	m_resourceDescriptions.push_back(resDesc);
	m_resourceUsageOffsets.push_back(static_cast<uint32_t>(m_resourceUsages.size()));
	m_resourceUsages.push_back({});

	return BufferHandle(m_resourceDescriptions.size());
}

VEngine::ImageHandle VEngine::RenderGraph::importImage(const ImageDescription &imageDescription, VkImage image, VkImageView imageView, VkImageLayout *layouts, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore)
{
	return ImageHandle();
}

VEngine::BufferHandle VEngine::RenderGraph::importBuffer(const BufferDescription &bufferDescription, VkBuffer buffer, VkDeviceSize offset, VKAllocationHandle allocation, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore)
{
	return BufferHandle();
}

void VEngine::RenderGraph::reset()
{
}
