#include "RenderGraph.h"
#include "Utility/Utility.h"
#include <cassert>
#include <memory>
#include <stack>
#include <algorithm>
#include "VKUtility.h"
#include "GlobalVar.h"

const VkQueue VEngine::RenderGraph::undefinedQueue = VkQueue(~size_t());

void VEngine::RenderGraph::createPasses(ResourceViewHandle finalResourceHandle, ResourceState finalResourceState, QueueType queueType)
{
	// add pass to transition final resource
	{
		if (finalResourceState != ResourceState::UNDEFINED)
		{
			const auto &viewDesc = m_viewDescriptions[(size_t)finalResourceHandle - 1];
			const size_t resourceIndex = (size_t)viewDesc.m_resourceHandle - 1;
			const auto &resDesc = m_resourceDescriptions[resourceIndex];
			const uint16_t passIndex = static_cast<uint16_t>(m_passSubresources.size());

			m_passNames.push_back("Final Layout Transition");
			m_passRecordInfo.push_back({});
			auto &recordInfo = m_passRecordInfo.back();
			recordInfo.m_queue = getQueue(queueType);
			recordInfo.m_recordFunc = [](VkCommandBuffer, const Registry &) {};

			m_passSubresources.push_back({ static_cast<uint32_t>(m_passSubresourceIndices.size()) });
			auto &passSubresources = m_passSubresources.back();
			passSubresources.m_readSubresourceCount = passSubresources.m_writeSubresourceCount = viewDesc.m_image ? viewDesc.m_subresourceRange.layerCount * viewDesc.m_subresourceRange.levelCount : 1;
			m_passSubresourceIndices.reserve(m_passSubresourceIndices.size() + passSubresources.m_readSubresourceCount + passSubresources.m_writeSubresourceCount);
			m_viewUsages.push_back({});

			const ResourceUsage resUsage{ passIndex, finalResourceState, finalResourceState };
			forEachSubresource(finalResourceHandle, [&](uint32_t index)
			{
				m_resourceUsages[index].push_back(resUsage);
				m_passSubresourceIndices.push_back(static_cast<uint16_t>(index));
			});
			forEachSubresource(finalResourceHandle, [&](uint32_t index)
			{
				m_passSubresourceIndices.push_back(static_cast<uint16_t>(index));
			});
		}
	}

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
		std::stack<uint32_t> resourceStack;
		for (uint32_t subresourceIdx = 0; subresourceIdx < m_resourceUsages.size(); ++subresourceIdx)
		{
			for (uint32_t usageIdx = 0; usageIdx < m_resourceUsages[subresourceIdx].size(); ++usageIdx)
			{
				if (getResourceStateInfo(m_resourceUsages[subresourceIdx][usageIdx].m_initialResourceState).m_readAccess)
				{
					++subresourceRefCounts[subresourceIdx];
				}
			}
			if (subresourceRefCounts[subresourceIdx] == 0)
			{
				resourceStack.push(subresourceIdx);
			}
		}

		// cull passes/resources
		while (!resourceStack.empty())
		{
			auto subresourceIndex = resourceStack.top();
			resourceStack.pop();

			// find writing passes
			for (const auto &usage : m_resourceUsages[subresourceIndex])
			{
				const auto passHandle = usage.m_passHandle;
				// if writing pass, decrement refCount. if it falls to zero, decrement refcount of read resources
				if (getResourceStateInfo(usage.m_initialResourceState).m_writeAccess && passRefCounts[passHandle] != 0 && --passRefCounts[passHandle] == 0)
				{
					--survivingPassCount;
					const uint32_t start = m_passSubresources[passHandle].m_subresourcesOffset;
					const uint32_t end = m_passSubresources[passHandle].m_subresourcesOffset + m_passSubresources[passHandle].m_readSubresourceCount;
					for (uint32_t i = start; i < end; ++i)
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

		// ensure passes with forced execution are not culled
		for (size_t i = 0; i < m_passSubresources.size(); ++i)
		{
			if (m_passSubresources[i].m_forceExecution)
			{
				++passRefCounts[i];
			}
		}

		// remove culled passes from resource usages
		for (auto &usages : m_resourceUsages)
		{
			usages.erase(std::remove_if(usages.begin(), usages.end(), [&](const auto &usage) {return passRefCounts[usage.m_passHandle] == 0; }), usages.end());
		}

		// determine resource lifetimes (up to this point pass handles are still ordered by the <-relation)
		std::vector<std::vector<uint32_t>> clearResources(passCount);
		for (uint32_t resourceIdx = 0; resourceIdx < m_resourceDescriptions.size(); ++resourceIdx)
		{
			std::pair<uint16_t, uint16_t> lifetime{ 0xFFFF, 0 };
			bool referenced = false;
			const uint32_t usageOffset = m_resourceUsageOffsets[resourceIdx];
			const uint32_t subresourceCount = m_resourceDescriptions[resourceIdx].m_subresourceCount;
			for (uint32_t subresourceIdx = usageOffset; subresourceIdx < subresourceCount + usageOffset; ++subresourceIdx)
			{
				if (!m_resourceUsages[subresourceIdx].empty())
				{
					referenced = true;
					lifetime.first = std::min(lifetime.first, m_resourceUsages[subresourceIdx].front().m_passHandle);
					lifetime.second = std::max(lifetime.second, m_resourceUsages[subresourceIdx].back().m_passHandle);
				}
			}
			if (m_resourceDescriptions[resourceIdx].m_clear && referenced)
			{
				clearResources[lifetime.first].push_back(resourceIdx);
			}
			m_resourceLifetimes[resourceIdx] = lifetime;
			m_culledResources[resourceIdx] = referenced;
		}

		// fill pass handles of surviving passes and insert clear passes
		std::vector<uint16_t> handleToIndex(passCount);
		m_passHandleOrder.reserve(survivingPassCount);
		for (uint16_t i = 0; i < passCount; ++i)
		{
			if (passRefCounts[i])
			{
				// insert clear pass
				if (!clearResources[i].empty())
				{
					const uint16_t clearPassHandle = static_cast<uint16_t>(m_passSubresources.size());

					m_passNames.push_back("Auto Clear Resource(s)");

					// add and populate PassSubresources struct
					m_passSubresources.push_back({ static_cast<uint32_t>(m_passSubresources.size()) });
					auto &passSubresource = m_passSubresources.back();
					for (const auto resourceIdx : clearResources[i])
					{
						const uint32_t usageOffset = m_resourceUsageOffsets[resourceIdx];
						const auto &resDesc = m_resourceDescriptions[resourceIdx];
						const uint32_t subresourceCount = resDesc.m_subresourceCount;
						// insert clear pass resource usage and add subresources to pass subresource index list
						for (uint32_t subresourceIdx = usageOffset; subresourceIdx < subresourceCount + usageOffset; ++subresourceIdx)
						{
							const auto state = resDesc.m_image ? ResourceState::WRITE_IMAGE_TRANSFER : ResourceState::WRITE_BUFFER_TRANSFER;
							m_resourceUsages[subresourceIdx].insert(m_resourceUsages[subresourceIdx].cbegin(), { clearPassHandle, state, state });
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
								const VkImage image = registry.getImage(ImageHandle((size_t)resourceIdx + 1));
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
								vkCmdFillBuffer(cmdBuf, registry.getBuffer(BufferHandle((size_t)resourceIdx + 1)), 0, resDesc.m_size, resDesc.m_clearValue.m_bufferClearValue);
							}
						}
					}, m_passRecordInfo[i].m_queue });
					m_viewUsages.push_back({});
					handleToIndex.push_back(static_cast<uint16_t>(m_passHandleOrder.size()));
					m_passHandleOrder.push_back(clearPassHandle);
				}
				handleToIndex[i] = static_cast<uint16_t>(m_passHandleOrder.size());
				m_passHandleOrder.push_back(i);
			}
		}

		// correct resource lifetimes to use indices into m_passHandleOrder
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
						else if (resDesc.m_imageType == VK_IMAGE_TYPE_3D && viewDesc.m_viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
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
		const auto &resDesc = m_resourceDescriptions[resourceIndex];

		if (!m_culledResources[resourceIndex] || resDesc.m_external)
		{
			continue;
		}

		VkFlags usageFlags = resDesc.m_usageFlags;

		// find usage flags and refCount
		const uint32_t subresourcesEndIndex = resDesc.m_subresourceCount + m_resourceUsageOffsets[resourceIndex];
		for (uint32_t i = m_resourceUsageOffsets[resourceIndex]; i < subresourcesEndIndex; ++i)
		{
			for (const auto &usage : m_resourceUsages[i])
			{
				usageFlags |= getResourceStateInfo(usage.m_initialResourceState).m_usageFlags;
				usageFlags |= getResourceStateInfo(usage.m_finalResourceState).m_usageFlags;
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
			if (resDesc.m_hostVisible)
			{
				allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
				allocCreateInfo.m_preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			}
			else
			{
				allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			}


			if (g_context.m_allocator.createBuffer(allocCreateInfo, bufferCreateInfo, m_imageBuffers[resourceIndex].buffer, m_allocations[resourceIndex]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create buffer!", EXIT_FAILURE);
			}
		}

		if (g_vulkanDebugCallBackEnabled)
		{
			VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
			info.objectType = resDesc.m_image ? VK_OBJECT_TYPE_IMAGE : VK_OBJECT_TYPE_BUFFER;
			info.objectHandle = resDesc.m_image ? (uint64_t)m_imageBuffers[resourceIndex].image : (uint64_t)m_imageBuffers[resourceIndex].buffer;
			info.pObjectName = resDesc.m_name;

			vkSetDebugUtilsObjectNameEXT(g_context.m_device, &info);
		}
	}

	// create views
	for (uint32_t viewIndex = 0; viewIndex < m_viewDescriptions.size(); ++viewIndex)
	{
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
			viewInfo.format = viewDesc.m_format == VK_FORMAT_UNDEFINED ? m_resourceDescriptions[(size_t)viewDesc.m_resourceHandle - 1].m_format : viewDesc.m_format;
			viewInfo.components = viewDesc.m_components;
			viewInfo.subresourceRange = viewDesc.m_subresourceRange;

			if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_imageBufferViews[viewIndex].imageView) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create image view!", EXIT_FAILURE);
			}

			if (g_vulkanDebugCallBackEnabled)
			{
				VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
				info.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
				info.objectHandle = (uint64_t)m_imageBufferViews[viewIndex].imageView;
				info.pObjectName = viewDesc.m_name;

				vkSetDebugUtilsObjectNameEXT(g_context.m_device, &info);
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

			if (g_vulkanDebugCallBackEnabled)
			{
				VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
				info.objectType = VK_OBJECT_TYPE_BUFFER_VIEW;
				info.objectHandle = (uint64_t)m_imageBufferViews[viewIndex].bufferView;
				info.pObjectName = viewDesc.m_name;

				vkSetDebugUtilsObjectNameEXT(g_context.m_device, &info);
			}
		}
	}
}

void VEngine::RenderGraph::createSynchronization(ResourceViewHandle finalResourceHandle, VkSemaphore finalResourceWaitSemaphore)
{
	struct SemaphoreDependencyInfo
	{
		VkPipelineStageFlags m_waitDstStageMask;
		uint16_t m_waitedOnPassHandle;
		uint16_t m_semaphoreIndex;
		bool m_external;
	};

	std::vector<std::pair<std::vector<SemaphoreDependencyInfo>, uint32_t>> semaphoreDependencies(m_passSubresources.size());
	VkPipelineStageFlags finalResourceWaitDstStageMask = 0;
	std::vector<bool> finalResourceSignalPasses(m_passSubresources.size());

	const auto &finalViewDesc = m_viewDescriptions[(size_t)finalResourceHandle - 1];
	const uint32_t finalResIdx = (uint32_t)(size_t)finalViewDesc.m_resourceHandle - 1;

	for (uint32_t resourceIdx = 0; resourceIdx < m_resourceDescriptions.size(); ++resourceIdx)
	{
		// skip culled resources
		if (!m_culledResources[resourceIdx])
		{
			continue;
		}

		const auto &resDesc = m_resourceDescriptions[resourceIdx];
		const auto usageOffset = m_resourceUsageOffsets[resourceIdx];

		for (uint32_t subresourceIdx = usageOffset; subresourceIdx < usageOffset + resDesc.m_subresourceCount; ++subresourceIdx)
		{
			if (m_resourceUsages[subresourceIdx].empty())
			{
				continue;
			}

			struct UsageInfo
			{
				uint16_t m_passHandle;
				VkQueue m_queue;
				ResourceStateInfo m_stateInfo;
			};

			VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			imageBarrier.image = m_imageBuffers[resourceIdx].image;

			VkBufferMemoryBarrier bufferBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
			bufferBarrier.buffer = m_imageBuffers[resourceIdx].buffer;
			bufferBarrier.offset = resDesc.m_offset;
			bufferBarrier.size = resDesc.m_size;

			const VkQueue firstUsageQueue = m_passRecordInfo[m_resourceUsages[subresourceIdx][0].m_passHandle].m_queue;
			const uint32_t relativeSubresIdx = subresourceIdx - usageOffset;
			UsageInfo prevUsageInfo{ 0, firstUsageQueue, getResourceStateInfo(ResourceState::UNDEFINED) };

			// if the resource is external, change prevUsageInfo accordingly, schedule a release barrier if required and update external info values
			if (resDesc.m_external)
			{
				const auto &extInfo = m_externalResourceInfo[resDesc.m_externalInfoIndex];
				const auto externalQueue = extInfo.m_queues ? extInfo.m_queues[relativeSubresIdx] : undefinedQueue;

				prevUsageInfo.m_queue = externalQueue != undefinedQueue ? externalQueue : prevUsageInfo.m_queue;
				prevUsageInfo.m_stateInfo = getResourceStateInfo(extInfo.m_resourceStates ? extInfo.m_resourceStates[relativeSubresIdx] : ResourceState::UNDEFINED);

				const uint16_t queueIdx = prevUsageInfo.m_queue == m_queues[0] ? 0 : prevUsageInfo.m_queue == m_queues[1] ? 1 : 2;

				prevUsageInfo.m_passHandle = queueIdx;

				// schedule release barrier if queues change
				if (prevUsageInfo.m_queue != firstUsageQueue)
				{
					const auto firstUsageStateInfo = getResourceStateInfo(m_resourceUsages[subresourceIdx][0].m_initialResourceState);
					m_externalReleaseMasks[queueIdx] |= prevUsageInfo.m_stateInfo.m_stageMask;
					if (resDesc.m_image)
					{
						// set image barrier values
						imageBarrier.srcAccessMask = prevUsageInfo.m_stateInfo.m_writeAccess ? prevUsageInfo.m_stateInfo.m_accessMask : 0;
						imageBarrier.dstAccessMask = 0;// firstUsageStateInfo.m_accessMask;
						imageBarrier.oldLayout = prevUsageInfo.m_stateInfo.m_layout;
						imageBarrier.newLayout = firstUsageStateInfo.m_layout;
						imageBarrier.srcQueueFamilyIndex = resDesc.m_concurrent ? VK_QUEUE_FAMILY_IGNORED : getQueueFamilyIndex(prevUsageInfo.m_queue);
						imageBarrier.dstQueueFamilyIndex = resDesc.m_concurrent ? VK_QUEUE_FAMILY_IGNORED : getQueueFamilyIndex(firstUsageQueue);
						imageBarrier.subresourceRange = { VKUtility::imageAspectMaskFromFormat(resDesc.m_format), relativeSubresIdx % resDesc.m_levels, 1, relativeSubresIdx / resDesc.m_levels, 1 };

						m_externalReleaseImageBarriers[queueIdx].push_back(imageBarrier);
					}
					else
					{
						// set buffer barrier values
						bufferBarrier.srcAccessMask = prevUsageInfo.m_stateInfo.m_writeAccess ? prevUsageInfo.m_stateInfo.m_accessMask : 0;
						bufferBarrier.dstAccessMask = 0;// firstUsageStateInfo.m_accessMask;
						bufferBarrier.srcQueueFamilyIndex = resDesc.m_concurrent ? VK_QUEUE_FAMILY_IGNORED : getQueueFamilyIndex(prevUsageInfo.m_queue);
						bufferBarrier.dstQueueFamilyIndex = resDesc.m_concurrent ? VK_QUEUE_FAMILY_IGNORED : getQueueFamilyIndex(firstUsageQueue);

						m_externalReleaseBufferBarriers[queueIdx].push_back(bufferBarrier);
					}
				}

				// update external info values
				const auto &lastUsage = m_resourceUsages[subresourceIdx].back();
				if (extInfo.m_queues)
				{
					extInfo.m_queues[relativeSubresIdx] = m_passRecordInfo[lastUsage.m_passHandle].m_queue;
				}
				if (extInfo.m_resourceStates)
				{
					extInfo.m_resourceStates[relativeSubresIdx] = lastUsage.m_finalResourceState;
				}
			}

			if (finalResIdx == resourceIdx)
			{
				finalResourceWaitDstStageMask |= getResourceStateInfo(m_resourceUsages[subresourceIdx][0].m_initialResourceState).m_stageMask;
			}

			const size_t usageCount = m_resourceUsages[subresourceIdx].size();
			for (size_t usageIdx = 0; usageIdx < usageCount; ++usageIdx)
			{
				const auto &subresUsage = m_resourceUsages[subresourceIdx][usageIdx];
				auto &passRecordInfo = m_passRecordInfo[subresUsage.m_passHandle];

				if (finalResIdx == resourceIdx && usageIdx == (usageCount - 1))
				{
					finalResourceSignalPasses[subresUsage.m_passHandle] = true;
				}

				UsageInfo curUsageInfo{};
				curUsageInfo.m_passHandle = subresUsage.m_passHandle;
				curUsageInfo.m_queue = passRecordInfo.m_queue;
				curUsageInfo.m_stateInfo = getResourceStateInfo(subresUsage.m_initialResourceState);

				bool scheduleSemaphore = prevUsageInfo.m_queue != curUsageInfo.m_queue;
				bool addReleaseBarrier = scheduleSemaphore && !resDesc.m_concurrent && !(resDesc.m_external && usageIdx == 0);
				bool addWaitBarrier = scheduleSemaphore && !resDesc.m_concurrent || resDesc.m_image && prevUsageInfo.m_stateInfo.m_layout != curUsageInfo.m_stateInfo.m_layout; // layout transitions always need a barrier

				// acquire resource + schedule wait on semaphore
				if (scheduleSemaphore)
				{
					bool externalDependency = usageIdx == 0 && resDesc.m_external;

					// try to merge dependency with existing dependencies
					bool foundExistingDependency = false;
					for (auto &dependency : semaphoreDependencies[curUsageInfo.m_passHandle].first)
					{
						if (dependency.m_waitedOnPassHandle == prevUsageInfo.m_passHandle && dependency.m_external == externalDependency)
						{
							dependency.m_waitDstStageMask |= curUsageInfo.m_stateInfo.m_stageMask;
							foundExistingDependency = true;
							break;
						}
					}
					if (!foundExistingDependency)
					{
						const uint16_t semaphoreIndex = externalDependency ? m_externalSemaphoreSignalCounts[prevUsageInfo.m_passHandle]++ : semaphoreDependencies[prevUsageInfo.m_passHandle].second++;
						semaphoreDependencies[curUsageInfo.m_passHandle].first.push_back({ curUsageInfo.m_stateInfo.m_stageMask, prevUsageInfo.m_passHandle, semaphoreIndex, externalDependency });
					}
				}
				else
				{
					if (prevUsageInfo.m_stateInfo.m_writeAccess)
					{
						passRecordInfo.m_memoryBarrierSrcAccessMask |= prevUsageInfo.m_stateInfo.m_accessMask;
						passRecordInfo.m_memoryBarrierDstAccessMask |= curUsageInfo.m_stateInfo.m_accessMask;
					}

					// schedule pipeline barrier for RAW and WAR hazards
					if (prevUsageInfo.m_stateInfo.m_writeAccess || curUsageInfo.m_stateInfo.m_writeAccess)
					{
						passRecordInfo.m_srcStageMask |= prevUsageInfo.m_stateInfo.m_stageMask;
						passRecordInfo.m_dstStageMask |= curUsageInfo.m_stateInfo.m_stageMask;
					}
				}

				if (addWaitBarrier)
				{
					passRecordInfo.m_dstStageMask |= curUsageInfo.m_stateInfo.m_stageMask;

					// set image barrier values
					imageBarrier.srcAccessMask = prevUsageInfo.m_stateInfo.m_writeAccess && !scheduleSemaphore ? prevUsageInfo.m_stateInfo.m_accessMask : 0;
					imageBarrier.dstAccessMask = curUsageInfo.m_stateInfo.m_accessMask;
					imageBarrier.oldLayout = prevUsageInfo.m_stateInfo.m_layout;
					imageBarrier.newLayout = curUsageInfo.m_stateInfo.m_layout;
					imageBarrier.srcQueueFamilyIndex = resDesc.m_concurrent ? VK_QUEUE_FAMILY_IGNORED : getQueueFamilyIndex(prevUsageInfo.m_queue);
					imageBarrier.dstQueueFamilyIndex = resDesc.m_concurrent ? VK_QUEUE_FAMILY_IGNORED : getQueueFamilyIndex(curUsageInfo.m_queue);
					imageBarrier.subresourceRange = { VKUtility::imageAspectMaskFromFormat(resDesc.m_format), relativeSubresIdx % resDesc.m_levels, 1, relativeSubresIdx / resDesc.m_levels, 1 };

					// set buffer barrier values
					bufferBarrier.srcAccessMask = imageBarrier.srcAccessMask;
					bufferBarrier.dstAccessMask = imageBarrier.dstAccessMask;
					bufferBarrier.srcQueueFamilyIndex = imageBarrier.srcQueueFamilyIndex;
					bufferBarrier.dstQueueFamilyIndex = imageBarrier.dstQueueFamilyIndex;

					if (resDesc.m_image)
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

				// add release barrier to previous pass
				if (addReleaseBarrier)
				{
					// set image barrier values
					imageBarrier.srcAccessMask = prevUsageInfo.m_stateInfo.m_writeAccess ? prevUsageInfo.m_stateInfo.m_accessMask : 0;
					imageBarrier.dstAccessMask = 0;// curUsageInfo.m_stateInfo.m_accessMask;
					imageBarrier.oldLayout = prevUsageInfo.m_stateInfo.m_layout;
					imageBarrier.newLayout = curUsageInfo.m_stateInfo.m_layout;
					imageBarrier.srcQueueFamilyIndex = resDesc.m_concurrent ? VK_QUEUE_FAMILY_IGNORED : getQueueFamilyIndex(prevUsageInfo.m_queue);
					imageBarrier.dstQueueFamilyIndex = resDesc.m_concurrent ? VK_QUEUE_FAMILY_IGNORED : getQueueFamilyIndex(curUsageInfo.m_queue);

					// set buffer barrier values
					bufferBarrier.srcAccessMask = imageBarrier.srcAccessMask;
					bufferBarrier.dstAccessMask = imageBarrier.dstAccessMask;
					bufferBarrier.srcQueueFamilyIndex = imageBarrier.srcQueueFamilyIndex;
					bufferBarrier.dstQueueFamilyIndex = imageBarrier.dstQueueFamilyIndex;

					auto &prevPassRecordInfo = m_passRecordInfo[prevUsageInfo.m_passHandle];

					if (resDesc.m_image)
					{
						prevPassRecordInfo.m_imageBarriers.push_back(imageBarrier);
						++prevPassRecordInfo.m_releaseImageBarrierCount;
					}
					else
					{
						prevPassRecordInfo.m_bufferBarriers.push_back(bufferBarrier);
						++prevPassRecordInfo.m_releaseBufferBarrierCount;
					}

					prevPassRecordInfo.m_releaseStageMask |= prevUsageInfo.m_stateInfo.m_stageMask;
				}

				// update prevUsageInfo
				prevUsageInfo = curUsageInfo;
				prevUsageInfo.m_stateInfo = getResourceStateInfo(subresUsage.m_finalResourceState);
			}
		}
	}

	// create semaphores and batches
	uint16_t externalSemaphoreOffsets[3];
	for (size_t i = 0; i < 3; ++i)
	{
		externalSemaphoreOffsets[i] = static_cast<uint32_t>(m_semaphores.size());
		for (size_t j = 0; j < m_externalSemaphoreSignalCounts[i]; ++j)
		{
			m_semaphores.push_back(g_context.m_syncPrimitivePool.acquireSemaphore());
		}
	}

	std::vector<uint16_t> semaphoreOffsets(m_passSubresources.size());
	VkQueue prevQueue = 0;
	bool startNewBatch = true;
	for (uint16_t i = 0; i < m_passHandleOrder.size(); ++i)
	{
		const uint16_t passHandle = m_passHandleOrder[i];
		const auto &semaphoreDependency = semaphoreDependencies[passHandle];
		VkQueue curQueue = m_passRecordInfo[passHandle].m_queue;
		const bool waitOnFinalResSemaphore = (m_resourceLifetimes[finalResIdx].first == i && finalResourceWaitSemaphore != VK_NULL_HANDLE);

		// if the previous pass needs to signal, startNewBatch is already true
		// if this pass needs to wait, we need a need batch
		// if the queue type changed, we also need to start a new batch
		// if this is the first pass to use the final resource and we need to wait on a semaphore...
		startNewBatch = startNewBatch || !semaphoreDependency.first.empty() || prevQueue != curQueue || waitOnFinalResSemaphore;
		if (startNewBatch)
		{
			startNewBatch = false;
			m_batches.push_back({});
			auto &batch = m_batches.back();
			batch.m_cmdBufOffset = 3 + i;
			batch.m_passIndexOffset = i;
			batch.m_queue = curQueue;
		}

		prevQueue = curQueue;

		// some other passes needs to wait on this one -> allocate semaphores, end batch after this pass
		if (semaphoreDependency.second || finalResourceSignalPasses[passHandle])
		{
			startNewBatch = true;
			semaphoreOffsets[passHandle] = static_cast<uint16_t>(m_semaphores.size());
			m_batches.back().m_signalSemaphoreOffset = semaphoreOffsets[passHandle];
			m_batches.back().m_signalSemaphoreCount = semaphoreDependency.second;
			for (uint32_t semaphoreIndex = 0; semaphoreIndex < semaphoreDependency.second; ++semaphoreIndex)
			{
				m_semaphores.push_back(g_context.m_syncPrimitivePool.acquireSemaphore());
			}
			if (finalResourceSignalPasses[passHandle])
			{
				++m_batches.back().m_signalSemaphoreCount;
				m_semaphores.push_back(g_context.m_syncPrimitivePool.acquireSemaphore());
				m_finalResourceSemaphores.push_back(m_semaphores.back());
			}
		}

		// this pass needs to wait on other passes -> fill in wait semaphores and waitDstMasks
		if (!semaphoreDependency.first.empty() || waitOnFinalResSemaphore)
		{
			auto &batch = m_batches.back();
			auto &waitSemaphores = batch.m_waitSemaphores;
			auto &waitDstMasks = batch.m_waitDstStageMasks;
			assert(waitSemaphores.size() == waitDstMasks.size());
			waitSemaphores.reserve(semaphoreDependency.first.size() + waitSemaphores.size() + (waitOnFinalResSemaphore ? 1 : 0));
			waitDstMasks.reserve(semaphoreDependency.first.size() + waitDstMasks.size() + (waitOnFinalResSemaphore ? 1 : 0));
			for (const auto &waitDependency : semaphoreDependency.first)
			{
				const auto offset = waitDependency.m_external ? externalSemaphoreOffsets[waitDependency.m_waitedOnPassHandle] : semaphoreOffsets[waitDependency.m_waitedOnPassHandle];
				waitSemaphores.push_back(m_semaphores[offset + waitDependency.m_semaphoreIndex]);
				waitDstMasks.push_back(waitDependency.m_waitDstStageMask);
			}
			if (waitOnFinalResSemaphore)
			{
				waitSemaphores.push_back(finalResourceWaitSemaphore);
				waitDstMasks.push_back(finalResourceWaitDstStageMask);
			}
		}

		++m_batches.back().m_passIndexCount;
	}

	// add fence for last batch on each queue
	for (size_t i = m_batches.size(); i--; )
	{
		auto &batch = m_batches[i];
		const size_t queueIdx = batch.m_queue == m_queues[0] ? 0 : batch.m_queue == m_queues[1] ? 1 : 2;
		if (m_fences[queueIdx] == VK_NULL_HANDLE)
		{
			m_fences[queueIdx] = g_context.m_syncPrimitivePool.acquireFence();
			batch.m_fence = m_fences[queueIdx];
		}
	}
}

void VEngine::RenderGraph::record()
{
	m_commandBuffers.resize(m_passHandleOrder.size() + 3);

	// release passes for external resources
	const char *releasePassNames[]
	{
		"release barriers for external resources (graphics queue)",
		"release barriers for external resources (compute queue)",
		"release barriers for external resources (transfer queue)",
	};
	for (size_t i = 0; i < 3; ++i)
	{
		if (m_externalSemaphoreSignalCounts[i])
		{
			if (!m_externalReleaseBufferBarriers[i].empty() || !m_externalReleaseImageBarriers[i].empty())
			{
				// get command buffer
				VkCommandBuffer cmdBuf = m_commandBuffers[i] = m_commandBufferPool.acquire(getQueueFamilyIndex(m_queues[i]));

				// start recording
				VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				vkBeginCommandBuffer(cmdBuf, &beginInfo);

				if (g_vulkanDebugCallBackEnabled)
				{
					VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
					label.pLabelName = releasePassNames[i];
					vkCmdInsertDebugUtilsLabelEXT(cmdBuf, &label);
				}

				vkCmdPipelineBarrier(cmdBuf, m_externalReleaseMasks[i], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr,
					static_cast<uint32_t>(m_externalReleaseBufferBarriers[i].size()), m_externalReleaseBufferBarriers[i].data(),
					static_cast<uint32_t>(m_externalReleaseImageBarriers[i].size()), m_externalReleaseImageBarriers[i].data());

				// end recording
				vkEndCommandBuffer(cmdBuf);
			}
		}
	}

	// each pass uses 4 queries, so make sure, we dont exceed the query pool size
	assert(!m_recordTimings || m_passHandleOrder.size() <= (TIMESTAMP_QUERY_COUNT / 4));

	bool resetQueryPools[3] = {};

	// record passes
	for (const auto &batch : m_batches)
	{
		for (uint16_t i = 0; i < batch.m_passIndexCount; ++i)
		{
			const uint16_t passHandle = m_passHandleOrder[i + batch.m_passIndexOffset];
			const auto &pass = m_passRecordInfo[passHandle];

			// get command buffer
			VkCommandBuffer cmdBuf = m_commandBuffers[batch.m_cmdBufOffset + i] = m_commandBufferPool.acquire(getQueueFamilyIndex(batch.m_queue));

			// start recording
			VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(cmdBuf, &beginInfo);

			if (g_vulkanDebugCallBackEnabled)
			{
				VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
				label.pLabelName = m_passNames[passHandle];
				vkCmdBeginDebugUtilsLabelEXT(cmdBuf, &label);
			}

			VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			memoryBarrier.srcAccessMask = pass.m_memoryBarrierSrcAccessMask;
			memoryBarrier.dstAccessMask = pass.m_memoryBarrierDstAccessMask;

			const uint32_t queryIndex = (i + batch.m_passIndexOffset) * 4;
			const uint32_t queueTypeIndex = batch.m_queue == m_queues[0] ? 0 : batch.m_queue == m_queues[1] ? 1 : 2;
			// disable timestamps if the queue doesnt support it
			const bool recordTimings = m_recordTimings && m_queueTimestampMasks[queueTypeIndex];

			if (recordTimings && !resetQueryPools[queueTypeIndex])
			{
				vkCmdResetQueryPool(cmdBuf, m_queryPools[queueTypeIndex], 0, TIMESTAMP_QUERY_COUNT);
				resetQueryPools[queueTypeIndex] = true;
			}

			// write timestamp before waits
			if (recordTimings)
			{
				vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPools[queueTypeIndex], queryIndex + 0);
			}

			// pipeline barrier
			if ((pass.m_waitBufferBarrierCount || pass.m_waitImageBarrierCount || memoryBarrier.srcAccessMask))
			{
				vkCmdPipelineBarrier(cmdBuf, (pass.m_srcStageMask != 0) ? pass.m_srcStageMask : pass.m_srcStageMask | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pass.m_dstStageMask, 0,
					1, &memoryBarrier,
					pass.m_waitBufferBarrierCount, pass.m_bufferBarriers.data(),
					pass.m_waitImageBarrierCount, pass.m_imageBarriers.data());
			}

			// write timestamp before pass commands
			if (recordTimings)
			{
				vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPools[queueTypeIndex], queryIndex + 1);
			}

			// record commands
			pass.m_recordFunc(cmdBuf, Registry(*this, i));

			// write timestamp after pass commands
			if (recordTimings)
			{
				vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPools[queueTypeIndex], queryIndex + 2);
			}

			// release resources
			if (pass.m_releaseBufferBarrierCount || pass.m_releaseImageBarrierCount)
			{
				vkCmdPipelineBarrier(cmdBuf, pass.m_releaseStageMask, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr,
					pass.m_releaseBufferBarrierCount, pass.m_bufferBarriers.data() + pass.m_waitBufferBarrierCount,
					pass.m_releaseImageBarrierCount, pass.m_imageBarriers.data() + pass.m_waitImageBarrierCount);
			}

			// write timestamp after all commands
			if (recordTimings)
			{
				vkCmdWriteTimestamp(cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPools[queueTypeIndex], queryIndex + 3);
			}

			if (g_vulkanDebugCallBackEnabled)
			{
				vkCmdEndDebugUtilsLabelEXT(cmdBuf);
			}

			// end recording
			vkEndCommandBuffer(cmdBuf);
		}
	}

	// insert external resource release batches
	{
		Batch releaseBatches[3]{};
		size_t batchCount = 0;
		uint16_t currentSignalSemaphoreOffset = 0;
		for (size_t i = 0; i < 3; ++i)
		{
			if (m_externalSemaphoreSignalCounts[i])
			{
				releaseBatches[batchCount].m_cmdBufOffset = static_cast<uint16_t>(i);
				releaseBatches[batchCount].m_passIndexCount = 1;
				releaseBatches[batchCount].m_queue = m_queues[i];
				releaseBatches[batchCount].m_signalSemaphoreOffset = currentSignalSemaphoreOffset;
				releaseBatches[batchCount].m_signalSemaphoreCount = m_externalSemaphoreSignalCounts[i];
				currentSignalSemaphoreOffset += m_externalSemaphoreSignalCounts[i];
				++batchCount;
			}
		}
		m_batches.insert(m_batches.begin(), releaseBatches, releaseBatches + batchCount);
	}

	// submit all batches
	for (const auto &batch : m_batches)
	{
		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = static_cast<uint32_t>(batch.m_waitSemaphores.size());
		submitInfo.pWaitSemaphores = batch.m_waitSemaphores.data();
		submitInfo.pWaitDstStageMask = batch.m_waitDstStageMasks.data();
		submitInfo.commandBufferCount = batch.m_passIndexCount;
		submitInfo.pCommandBuffers = m_commandBuffers.data() + batch.m_cmdBufOffset;
		submitInfo.signalSemaphoreCount = batch.m_signalSemaphoreCount;
		submitInfo.pSignalSemaphores = m_semaphores.data() + batch.m_signalSemaphoreOffset;

		if (vkQueueSubmit(batch.m_queue, 1, &submitInfo, batch.m_fence) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to submit batch to queue!", EXIT_FAILURE);
		}
	}
}

uint32_t VEngine::RenderGraph::getQueueFamilyIndex(VkQueue queue) const
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

VkQueue VEngine::RenderGraph::getQueue(QueueType queueType) const
{
	return m_queues[static_cast<size_t>(queueType)];
}

VEngine::RenderGraph::ResourceStateInfo VEngine::RenderGraph::getResourceStateInfo(ResourceState resourceState) const
{
	switch (resourceState)
	{
	case VEngine::ResourceState::UNDEFINED:
		return { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED, 0, false, false };
	case VEngine::ResourceState::READ_DEPTH_STENCIL:
		return { VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, true, false };
	case VEngine::ResourceState::READ_ATTACHMENT:
		return { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, true, false };
	case VEngine::ResourceState::READ_TEXTURE_VERTEX_SHADER:
		return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, true, false };
	case VEngine::ResourceState::READ_TEXTURE_TESSC_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, true, false };
	case VEngine::ResourceState::READ_TEXTURE_TESSE_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, true, false };
	case VEngine::ResourceState::READ_TEXTURE_GEOMETRY_SHADER:
		return { VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, true, false };
	case VEngine::ResourceState::READ_TEXTURE_FRAGMENT_SHADER:
		return { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, true, false };
	case VEngine::ResourceState::READ_TEXTURE_COMPUTE_SHADER:
		return { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_IMAGE_VERTEX_SHADER:
		return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_IMAGE_TESSC_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_IMAGE_TESSE_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_IMAGE_GEOMETRY_SHADER:
		return { VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_IMAGE_FRAGMENT_SHADER:
		return { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_IMAGE_COMPUTE_SHADER:
		return { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_BUFFER_VERTEX_SHADER:
		return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_BUFFER_TESSC_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_BUFFER_TESSE_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_BUFFER_GEOMETRY_SHADER:
		return { VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_BUFFER_FRAGMENT_SHADER:
		return { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_STORAGE_BUFFER_COMPUTE_SHADER:
		return { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_UNIFORM_BUFFER_VERTEX_SHADER:
		return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_UNIFORM_BUFFER_TESSC_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_UNIFORM_BUFFER_TESSE_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_UNIFORM_BUFFER_GEOMETRY_SHADER:
		return { VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_UNIFORM_BUFFER_FRAGMENT_SHADER:
		return { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_UNIFORM_BUFFER_COMPUTE_SHADER:
		return { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_VERTEX_BUFFER:
		return { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_INDEX_BUFFER:
		return { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_INDIRECT_BUFFER:
		return { VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, true, false };
	case VEngine::ResourceState::READ_BUFFER_TRANSFER:
		return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, false };
	case VEngine::ResourceState::READ_IMAGE_TRANSFER:
		return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT, true, false };
	case VEngine::ResourceState::READ_WRITE_STORAGE_IMAGE_VERTEX_SHADER:
		return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_IMAGE_TESSC_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_IMAGE_TESSE_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_IMAGE_GEOMETRY_SHADER:
		return { VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_IMAGE_FRAGMENT_SHADER:
		return { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER:
		return { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_BUFFER_VERTEX_SHADER:
		return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_BUFFER_TESSC_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_BUFFER_TESSE_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_BUFFER_GEOMETRY_SHADER:
		return { VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_BUFFER_FRAGMENT_SHADER:
		return { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER:
		return { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true, true };
	case VEngine::ResourceState::READ_WRITE_ATTACHMENT:
		return { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, true, true };
	case VEngine::ResourceState::WRITE_DEPTH_STENCIL:
		return { VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, false, true };
	case VEngine::ResourceState::WRITE_ATTACHMENT:
		return { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_IMAGE_VERTEX_SHADER:
		return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_IMAGE_TESSC_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_IMAGE_TESSE_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_IMAGE_GEOMETRY_SHADER:
		return { VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_IMAGE_FRAGMENT_SHADER:
		return { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_IMAGE_COMPUTE_SHADER:
		return { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_USAGE_STORAGE_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_BUFFER_VERTEX_SHADER:
		return { VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_BUFFER_TESSC_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_BUFFER_TESSE_SHADER:
		return { VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_BUFFER_GEOMETRY_SHADER:
		return { VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,  VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_BUFFER_FRAGMENT_SHADER:
		return { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false, true };
	case VEngine::ResourceState::WRITE_STORAGE_BUFFER_COMPUTE_SHADER:
		return { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false, true };
	case VEngine::ResourceState::WRITE_BUFFER_TRANSFER:
		return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, true };
	case VEngine::ResourceState::WRITE_IMAGE_TRANSFER:
		return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT, false, true };
	case VEngine::ResourceState::PRESENT_IMAGE:
		return { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, true, false };
	default:
		assert(false);
		break;
	}
	return ResourceStateInfo();
}

void VEngine::RenderGraph::forEachSubresource(ResourceViewHandle handle, std::function<void(uint32_t)> func)
{
	const size_t viewIndex = (size_t)handle - 1;
	const auto &viewDesc = m_viewDescriptions[viewIndex];
	const size_t resIndex = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_resourceDescriptions[resIndex];
	const uint32_t resourceUsagesOffset = m_resourceUsageOffsets[resIndex];

	if (resDesc.m_image)
	{
		const uint32_t baseLayer = viewDesc.m_subresourceRange.baseArrayLayer;
		const uint32_t layerCount = viewDesc.m_subresourceRange.layerCount;
		const uint32_t baseLevel = viewDesc.m_subresourceRange.baseMipLevel;
		const uint32_t levelCount = viewDesc.m_subresourceRange.levelCount;
		for (uint32_t layer = 0; layer < layerCount; ++layer)
		{
			for (uint32_t level = 0; level < levelCount; ++level)
			{
				const uint32_t index = (layer + baseLayer) * resDesc.m_levels + (level + baseLevel) + resourceUsagesOffset;
				func(index);
			}
		}
	}
	else
	{
		for (uint32_t i = 0; i < resDesc.m_subresourceCount; ++i)
		{
			func(resourceUsagesOffset + i);
		}
	}
}

VEngine::RenderGraph::RenderGraph()
{
	m_queues[0] = g_context.m_graphicsQueue;
	m_queues[1] = g_context.m_computeQueue;
	m_queues[2] = g_context.m_transferQueue;
	m_queueFamilyIndices[0] = g_context.m_queueFamilyIndices.m_graphicsFamily;
	m_queueFamilyIndices[1] = g_context.m_queueFamilyIndices.m_computeFamily;
	m_queueFamilyIndices[2] = g_context.m_queueFamilyIndices.m_transferFamily;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(g_context.m_physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(g_context.m_physicalDevice, &queueFamilyCount, queueFamilies.data());
	for (size_t i = 0; i < 3; ++i)
	{
		const uint32_t validBits = queueFamilies[m_queueFamilyIndices[i]].timestampValidBits;
		m_queueTimestampMasks[i] = validBits >= 64 ? ~uint64_t() : ((uint64_t(1) << validBits) - 1);
	}

	VkQueryPoolCreateInfo queryPoolCreateInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolCreateInfo.queryCount = TIMESTAMP_QUERY_COUNT; // TODO: make this dynamic?

	for (size_t i = 0; i < 3; ++i)
	{
		if (vkCreateQueryPool(g_context.m_device, &queryPoolCreateInfo, nullptr, &m_queryPools[i]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create query pool!", EXIT_FAILURE);
		}
	}

	m_timingInfos = std::make_unique<PassTimingInfo[]>(TIMESTAMP_QUERY_COUNT);
}

VEngine::RenderGraph::~RenderGraph()
{
	reset();
}

void VEngine::RenderGraph::addPass(const char *name, QueueType queueType, uint32_t passResourceUsageCount, const ResourceUsageDescription *passResourceUsages, const RecordFunc &recordFunc, bool forceExecution)
{
	const uint16_t passIndex = static_cast<uint16_t>(m_passSubresources.size());

	m_passNames.push_back(name);

	m_passRecordInfo.push_back({});
	auto &recordInfo = m_passRecordInfo.back();
	recordInfo.m_queue = getQueue(queueType);
	recordInfo.m_recordFunc = recordFunc;

	m_passSubresources.push_back({ static_cast<uint32_t>(m_passSubresourceIndices.size()) });
	auto &passSubresources = m_passSubresources.back();
	passSubresources.m_forceExecution = forceExecution;

	m_viewUsages.push_back({});

	// determine number of read- and write resources and update resource usages
	for (uint32_t i = 0; i < passResourceUsageCount; ++i)
	{
		const auto &usage = passResourceUsages[i];
		const auto &viewDesc = m_viewDescriptions[(size_t)usage.m_viewHandle - 1];
		const size_t resourceIndex = (size_t)viewDesc.m_resourceHandle - 1;
		const auto &resDesc = m_resourceDescriptions[resourceIndex];

		m_viewUsages[passIndex].push_back((uint16_t)(size_t)usage.m_viewHandle - 1);

		const auto stateInfo = getResourceStateInfo(usage.m_resourceState);
		const auto viewSubresourceCount = viewDesc.m_image ? viewDesc.m_subresourceRange.layerCount * viewDesc.m_subresourceRange.levelCount : 1;
		passSubresources.m_readSubresourceCount += stateInfo.m_readAccess ? viewSubresourceCount : 0;
		passSubresources.m_writeSubresourceCount += stateInfo.m_writeAccess ? viewSubresourceCount : 0;

		const ResourceUsage resUsage{ passIndex,usage.m_resourceState, usage.m_customUsage ? usage.m_finalResourceState : usage.m_resourceState };
		forEachSubresource(usage.m_viewHandle, [&](uint32_t index) {m_resourceUsages[index].push_back(resUsage); });
	}

	m_passSubresourceIndices.reserve(m_passSubresourceIndices.size() + passSubresources.m_readSubresourceCount + passSubresources.m_writeSubresourceCount);

	// add read resource indices to list and update resource usages
	for (uint32_t i = 0; i < passResourceUsageCount; ++i)
	{
		const auto &usage = passResourceUsages[i];
		if (getResourceStateInfo(usage.m_resourceState).m_readAccess)
		{
			forEachSubresource(usage.m_viewHandle, [&](uint32_t index) { m_passSubresourceIndices.push_back(static_cast<uint16_t>(index)); });
		}
	}
	// add write resources
	for (uint32_t i = 0; i < passResourceUsageCount; ++i)
	{
		const auto &usage = passResourceUsages[i];
		if (getResourceStateInfo(usage.m_resourceState).m_writeAccess)
		{
			forEachSubresource(usage.m_viewHandle, [&](uint32_t index) { m_passSubresourceIndices.push_back(static_cast<uint16_t>(index)); });
		}
	}
}

VEngine::ImageViewHandle VEngine::RenderGraph::createImageView(const ImageViewDescription &viewDesc)
{
	const auto &resDesc = m_resourceDescriptions[(size_t)viewDesc.m_imageHandle - 1];
	ResourceViewDescription desc{};
	desc.m_name = viewDesc.m_name;
	desc.m_resourceHandle = ResourceHandle(viewDesc.m_imageHandle);
	desc.m_viewType = viewDesc.m_viewType;
	desc.m_format = viewDesc.m_format == VK_FORMAT_UNDEFINED ? resDesc.m_format : viewDesc.m_format;
	desc.m_components = viewDesc.m_components;
	desc.m_subresourceRange = viewDesc.m_subresourceRange;
	desc.m_subresourceRange.levelCount = desc.m_subresourceRange.levelCount == VK_REMAINING_MIP_LEVELS ? resDesc.m_levels - desc.m_subresourceRange.baseMipLevel : desc.m_subresourceRange.levelCount;
	desc.m_subresourceRange.layerCount = desc.m_subresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS ? resDesc.m_layers - desc.m_subresourceRange.baseArrayLayer : desc.m_subresourceRange.layerCount;
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
	desc.m_range = viewDesc.m_range == VK_WHOLE_SIZE ? m_resourceDescriptions[(size_t)viewDesc.m_bufferHandle - 1].m_size : viewDesc.m_range;

	m_viewDescriptions.push_back(desc);

	return BufferViewHandle(m_viewDescriptions.size());
}

VEngine::ImageHandle VEngine::RenderGraph::createImage(const ImageDescription &imageDesc)
{
	ResourceDescription resDesc{};
	resDesc.m_name = imageDesc.m_name;
	resDesc.m_usageFlags = imageDesc.m_usageFlags | (imageDesc.m_clear ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0);
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
	resDesc.m_concurrent = imageDesc.m_concurrent;
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
	resDesc.m_usageFlags = bufferDesc.m_usageFlags | (bufferDesc.m_clear ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0);
	resDesc.m_clear = bufferDesc.m_clear;
	resDesc.m_clearValue = bufferDesc.m_clearValue;
	resDesc.m_offset = 0;
	resDesc.m_size = bufferDesc.m_size;
	resDesc.m_subresourceCount = 1;
	resDesc.m_concurrent = bufferDesc.m_concurrent;
	resDesc.m_hostVisible = bufferDesc.m_hostVisible;

	m_resourceDescriptions.push_back(resDesc);
	m_resourceUsageOffsets.push_back(static_cast<uint32_t>(m_resourceUsages.size()));
	m_resourceUsages.push_back({});

	return BufferHandle(m_resourceDescriptions.size());
}

VEngine::ImageHandle VEngine::RenderGraph::importImage(const ImageDescription &imageDesc, VkImage image, VkQueue *queues, ResourceState *resourceStates)
{
	ResourceDescription resDesc{};
	resDesc.m_name = imageDesc.m_name;
	resDesc.m_usageFlags = imageDesc.m_usageFlags | (imageDesc.m_clear ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0);
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
	resDesc.m_externalInfoIndex = static_cast<uint16_t>(m_externalResourceInfo.size());
	resDesc.m_concurrent = imageDesc.m_concurrent;
	resDesc.m_external = true;
	resDesc.m_image = true;

	m_resourceDescriptions.push_back(resDesc);
	m_resourceUsageOffsets.push_back(static_cast<uint32_t>(m_resourceUsages.size()));
	for (size_t i = 0; i < resDesc.m_subresourceCount; ++i)
	{
		m_resourceUsages.push_back({});
	}

	m_imageBuffers.resize(m_resourceDescriptions.size());
	m_imageBuffers.back().image = image;

	m_externalResourceInfo.push_back({ queues, resourceStates });

	if (g_vulkanDebugCallBackEnabled)
	{
		VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
		info.objectType = VK_OBJECT_TYPE_IMAGE;
		info.objectHandle = (uint64_t)image;
		info.pObjectName = imageDesc.m_name;

		vkSetDebugUtilsObjectNameEXT(g_context.m_device, &info);
	}

	return ImageHandle(m_resourceDescriptions.size());
}

VEngine::BufferHandle VEngine::RenderGraph::importBuffer(const BufferDescription &bufferDesc, VkBuffer buffer, VkDeviceSize offset, VkQueue *queue, ResourceState *resourceState)
{
	ResourceDescription resDesc{};
	resDesc.m_name = bufferDesc.m_name;
	resDesc.m_usageFlags = bufferDesc.m_usageFlags | (bufferDesc.m_clear ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0);
	resDesc.m_clear = bufferDesc.m_clear;
	resDesc.m_clearValue = bufferDesc.m_clearValue;
	resDesc.m_offset = offset;
	resDesc.m_size = bufferDesc.m_size;
	resDesc.m_subresourceCount = 1;
	resDesc.m_externalInfoIndex = static_cast<uint16_t>(m_externalResourceInfo.size());
	resDesc.m_concurrent = bufferDesc.m_concurrent;
	resDesc.m_external = true;

	m_resourceDescriptions.push_back(resDesc);
	m_resourceUsageOffsets.push_back(static_cast<uint32_t>(m_resourceUsages.size()));
	m_resourceUsages.push_back({});

	m_imageBuffers.resize(m_resourceDescriptions.size());
	m_imageBuffers.back().buffer = buffer;

	m_externalResourceInfo.push_back({ queue, resourceState });

	if (g_vulkanDebugCallBackEnabled)
	{
		VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
		info.objectType = VK_OBJECT_TYPE_BUFFER;
		info.objectHandle = (uint64_t)buffer;
		info.pObjectName = bufferDesc.m_name;

		vkSetDebugUtilsObjectNameEXT(g_context.m_device, &info);
	}

	return BufferHandle(m_resourceDescriptions.size());
}

void VEngine::RenderGraph::reset()
{
	// wait on fences
	{
		VkFence fences[3];
		uint32_t fenceCount = 0;
		for (size_t i = 0; i < 3; ++i)
		{
			if (m_fences[i] != VK_NULL_HANDLE)
			{
				fences[fenceCount++] = m_fences[i];
			}
		}
		if (fenceCount)
		{
			if (vkWaitForFences(g_context.m_device, fenceCount, fences, VK_TRUE, std::numeric_limits<uint64_t>::max()) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to wait on fences!", EXIT_FAILURE);
			}
		}
	}

	assert(g_context.m_syncPrimitivePool.checkIntegrity());

	// release semaphores
	for (auto semaphore : m_semaphores)
	{
		if (semaphore != VK_NULL_HANDLE)
		{
			g_context.m_syncPrimitivePool.freeSemaphore(semaphore);
		}
	}

	// release fences
	for (size_t i = 0; i < 3; ++i)
	{
		if (m_fences[i] != VK_NULL_HANDLE)
		{
			g_context.m_syncPrimitivePool.freeFence(m_fences[i]);
		}
	}

	// destroy internal resources
	for (size_t i = 0; i < m_resourceDescriptions.size(); ++i)
	{
		const auto &resDesc = m_resourceDescriptions[i];
		if (m_culledResources[i] && !resDesc.m_external)
		{
			if (resDesc.m_image)
			{
				g_context.m_allocator.destroyImage(m_imageBuffers[i].image, m_allocations[i]);
			}
			else
			{
				g_context.m_allocator.destroyBuffer(m_imageBuffers[i].buffer, m_allocations[i]);
			}
		}
	}

	// destroy views
	for (size_t i = 0; i < m_viewDescriptions.size(); ++i)
	{
		if (m_culledViews[i])
		{
			if (m_viewDescriptions[i].m_image)
			{
				vkDestroyImageView(g_context.m_device, m_imageBufferViews[i].imageView, nullptr);
			}
			else if (m_imageBufferViews[i].bufferView != VK_NULL_HANDLE)
			{
				vkDestroyBufferView(g_context.m_device, m_imageBufferViews[i].bufferView, nullptr);
			}
		}
	}

	if (m_recordTimings && !m_passHandleOrder.empty())
	{
		m_timingInfoCount = static_cast<uint32_t>(m_passHandleOrder.size());
		for (uint32_t i = 0; i < m_passHandleOrder.size(); ++i)
		{
			const VkQueue queue = m_passRecordInfo[m_passHandleOrder[i]].m_queue;
			const uint32_t queueIndex = queue == m_queues[0] ? 0 : queue == m_queues[1] ? 1 : 2;

			// zero initialize data so that passes on queues without timestamp support will have timings of 0
			uint64_t data[4]{};
			if (m_queueTimestampMasks[queueIndex])
			{
				if (vkGetQueryPoolResults(g_context.m_device, m_queryPools[queueIndex], i * 4, 4, sizeof(data), data, sizeof(data[0]), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS)
				{
					assert(false);
				}

				// mask value by valid timestamp bits
				for (size_t j = 0; j < 4; ++j)
				{
					data[j] = data[j] & m_queueTimestampMasks[queueIndex];
				}
			}

			m_timingInfos[i] =
			{
				m_passNames[m_passHandleOrder[i]],
				static_cast<float>((data[2] - data[1]) * (g_context.m_properties.limits.timestampPeriod * (1.0 / 1e6))),
				static_cast<float>((data[3] - data[0]) * (g_context.m_properties.limits.timestampPeriod * (1.0 / 1e6))),
			};
		}
	}

	// clear vectors
	{
		m_passNames.clear();
		m_passSubresourceIndices.clear();
		m_passSubresources.clear();
		m_resourceDescriptions.clear();
		m_externalResourceInfo.clear();
		m_resourceUsages.clear();
		m_resourceUsageOffsets.clear();
		m_resourceLifetimes.clear();
		m_culledResources.clear();
		m_viewDescriptions.clear();
		m_viewUsages.clear();
		m_culledViews.clear();
		m_allocations.clear();
		m_imageBuffers.clear();
		m_imageBufferViews.clear();
		m_semaphores.clear();
		m_finalResourceSemaphores.clear();
		m_passHandleOrder.clear();
		m_commandBuffers.clear();
		m_batches.clear();
		m_passRecordInfo.clear();
		memset(m_fences, 0, sizeof(m_fences));
		memset(m_externalSemaphoreSignalCounts, 0, sizeof(m_externalSemaphoreSignalCounts));
		memset(m_externalReleaseMasks, 0, sizeof(m_externalReleaseMasks));
		for (size_t i = 0; i < 3; ++i)
		{
			m_externalReleaseImageBarriers[i].clear();
			m_externalReleaseBufferBarriers[i].clear();
		}
	}

	m_commandBufferPool.reset();
}

void VEngine::RenderGraph::execute(ResourceViewHandle finalResourceHandle, VkSemaphore waitSemaphore, uint32_t *signalSemaphoreCount, VkSemaphore **signalSemaphores, ResourceState finalResourceState, QueueType queueType)
{
	createPasses(finalResourceHandle, finalResourceState, queueType);
	createResources();
	createSynchronization(finalResourceHandle, waitSemaphore);
	record();
	*signalSemaphoreCount = static_cast<uint32_t>(m_finalResourceSemaphores.size());
	*signalSemaphores = m_finalResourceSemaphores.data();
}

void VEngine::RenderGraph::getTimingInfo(size_t *count, const PassTimingInfo **data) const
{
	*count = m_timingInfoCount;
	*data = m_timingInfos.get();
}

VEngine::Registry::Registry(const RenderGraph &graph, uint16_t passHandleIndex)
	:m_graph(graph),
	m_passHandleIndex(passHandleIndex)
{
}

VkImage VEngine::Registry::getImage(ImageHandle handle) const
{
	return m_graph.m_imageBuffers[(size_t)handle - 1].image;
}

VkImage VEngine::Registry::getImage(ImageViewHandle handle) const
{
	return m_graph.m_imageBuffers[(size_t)m_graph.m_viewDescriptions[(size_t)handle - 1].m_resourceHandle - 1].image;
}

VkImageView VEngine::Registry::getImageView(ImageViewHandle handle) const
{
	return m_graph.m_imageBufferViews[(size_t)handle - 1].imageView;
}

VkDescriptorImageInfo VEngine::Registry::getImageInfo(ImageViewHandle handle, ResourceState state, VkSampler sampler) const
{
	return { sampler, m_graph.m_imageBufferViews[(size_t)handle - 1].imageView, m_graph.getResourceStateInfo(state).m_layout };
}

VEngine::ImageViewDescription VEngine::Registry::getImageViewDescription(ImageViewHandle handle) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	ImageViewDescription imageViewDesc{};
	imageViewDesc.m_name = viewDesc.m_name;
	imageViewDesc.m_imageHandle = ImageHandle(viewDesc.m_resourceHandle);
	imageViewDesc.m_viewType = viewDesc.m_viewType;
	imageViewDesc.m_format = viewDesc.m_format;
	imageViewDesc.m_components = viewDesc.m_components;
	imageViewDesc.m_subresourceRange = viewDesc.m_subresourceRange;

	return imageViewDesc;
}

VEngine::ImageDescription VEngine::Registry::getImageDescription(ImageViewHandle handle) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	const auto &resDesc = m_graph.m_resourceDescriptions[(size_t)viewDesc.m_resourceHandle - 1];
	ImageDescription imageDesc{};
	imageDesc.m_name = resDesc.m_name;
	imageDesc.m_concurrent = resDesc.m_concurrent;
	imageDesc.m_clear = resDesc.m_clear;
	imageDesc.m_clearValue = resDesc.m_clearValue;
	imageDesc.m_width = resDesc.m_width;
	imageDesc.m_height = resDesc.m_height;
	imageDesc.m_depth = resDesc.m_depth;
	imageDesc.m_layers = resDesc.m_layers;
	imageDesc.m_levels = resDesc.m_levels;
	imageDesc.m_samples = resDesc.m_samples;
	imageDesc.m_imageType = resDesc.m_imageType;
	imageDesc.m_format = resDesc.m_format;
	imageDesc.m_usageFlags = resDesc.m_usageFlags;

	return imageDesc;
}

VkAttachmentDescription VEngine::Registry::getAttachmentDescription(ImageViewHandle handle, ResourceState state, bool clear) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	const size_t resIdx = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_graph.m_resourceDescriptions[resIdx];
	const auto &lifetime = m_graph.m_resourceLifetimes[resIdx];
	const bool isFirstUse = lifetime.first == m_passHandleIndex;
	const bool isLastUse = lifetime.second == m_passHandleIndex;

	VkAttachmentDescription desc{};
	desc.format = viewDesc.m_format;
	desc.samples = VkSampleCountFlagBits(resDesc.m_samples);
	desc.loadOp = isFirstUse && clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : isFirstUse ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_LOAD;
	desc.storeOp = isLastUse && !resDesc.m_external ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
	desc.stencilLoadOp = desc.loadOp;
	desc.stencilStoreOp = desc.storeOp;
	desc.initialLayout = m_graph.getResourceStateInfo(state).m_layout;
	desc.finalLayout = desc.initialLayout;

	return desc;
}

VkImageLayout VEngine::Registry::getLayout(ResourceState state) const
{
	return m_graph.getResourceStateInfo(state).m_layout;
}

VkBuffer VEngine::Registry::getBuffer(BufferHandle handle) const
{
	return m_graph.m_imageBuffers[(size_t)handle - 1].buffer;
}

VkBuffer VEngine::Registry::getBuffer(BufferViewHandle handle) const
{
	return m_graph.m_imageBuffers[(size_t)m_graph.m_viewDescriptions[(size_t)handle - 1].m_resourceHandle - 1].buffer;
}

VkBufferView VEngine::Registry::getBufferView(BufferViewHandle handle) const
{
	return m_graph.m_imageBufferViews[(size_t)handle - 1].bufferView;
}

VkDescriptorBufferInfo VEngine::Registry::getBufferInfo(BufferViewHandle handle) const
{
	const size_t viewIdx = (size_t)handle - 1;
	const auto &viewDesc = m_graph.m_viewDescriptions[viewIdx];
	const size_t resIdx = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_graph.m_resourceDescriptions[resIdx];
	return { m_graph.m_imageBuffers[resIdx].buffer, resDesc.m_offset + viewDesc.m_offset, viewDesc.m_range };
}

VEngine::BufferViewDescription VEngine::Registry::getBufferViewDescription(BufferViewHandle handle) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	BufferViewDescription bufferViewDesc{};
	bufferViewDesc.m_name = viewDesc.m_name;
	bufferViewDesc.m_bufferHandle = BufferHandle(viewDesc.m_resourceHandle);
	bufferViewDesc.m_format = viewDesc.m_format;
	bufferViewDesc.m_offset = viewDesc.m_offset;
	bufferViewDesc.m_range = viewDesc.m_range;

	return bufferViewDesc;
}

VEngine::BufferDescription VEngine::Registry::getBufferDescription(BufferViewHandle handle) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	const auto &resDesc = m_graph.m_resourceDescriptions[(size_t)viewDesc.m_resourceHandle - 1];
	BufferDescription bufferDesc{};
	bufferDesc.m_name = resDesc.m_name;
	bufferDesc.m_concurrent = resDesc.m_concurrent;
	bufferDesc.m_clear = resDesc.m_clear;
	bufferDesc.m_clearValue = resDesc.m_clearValue;
	bufferDesc.m_size = resDesc.m_size;
	bufferDesc.m_usageFlags = resDesc.m_usageFlags;

	return bufferDesc;
}

bool VEngine::Registry::firstUse(ResourceHandle handle) const
{
	return m_passHandleIndex == m_graph.m_resourceLifetimes[(size_t)handle - 1].first;
}

bool VEngine::Registry::lastUse(ResourceHandle handle) const
{
	return m_passHandleIndex == m_graph.m_resourceLifetimes[(size_t)handle - 1].second;
}

void VEngine::Registry::map(BufferViewHandle handle, void **data) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	const size_t resIdx = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_graph.m_resourceDescriptions[resIdx];
	assert(resDesc.m_hostVisible && !resDesc.m_external);
	g_context.m_allocator.mapMemory(m_graph.m_allocations[resIdx], data);
	*data = ((uint8_t*)*data) + viewDesc.m_offset;
}

void VEngine::Registry::unmap(BufferViewHandle handle) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	const size_t resIdx = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_graph.m_resourceDescriptions[resIdx];
	assert(resDesc.m_hostVisible && !resDesc.m_external);
	g_context.m_allocator.unmapMemory(m_graph.m_allocations[resIdx]);
}
