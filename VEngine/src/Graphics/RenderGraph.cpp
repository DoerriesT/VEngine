#include "RenderGraph.h"
#include <cassert>
#include <stack>
#include <algorithm>
#include "gal/Initializers.h"

using namespace VEngine::gal;

VEngine::rg::Registry::Registry(const RenderGraph &graph, uint16_t passHandleIndex)
	:m_graph(graph),
	m_passHandleIndex(passHandleIndex)
{
}

Image *VEngine::rg::Registry::getImage(ImageHandle handle) const
{
	return m_graph.m_imageBuffers[(size_t)handle - 1].image;
}

Image *VEngine::rg::Registry::getImage(ImageViewHandle handle) const
{
	return m_graph.m_imageBuffers[(size_t)m_graph.m_viewDescriptions[(size_t)handle - 1].m_resourceHandle - 1].image;
}

ImageView *VEngine::rg::Registry::getImageView(ImageViewHandle handle) const
{
	return m_graph.m_imageBufferViews[(size_t)handle - 1].imageView;
}

VEngine::rg::ImageViewDescription VEngine::rg::Registry::getImageViewDescription(ImageViewHandle handle) const
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

VEngine::rg::ImageDescription VEngine::rg::Registry::getImageDescription(ImageViewHandle handle) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	const auto &resDesc = m_graph.m_resourceDescriptions[(size_t)viewDesc.m_resourceHandle - 1];
	ImageDescription imageDesc{};
	imageDesc.m_name = resDesc.m_name;
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

Buffer *VEngine::rg::Registry::getBuffer(BufferHandle handle) const
{
	return m_graph.m_imageBuffers[(size_t)handle - 1].buffer;
}

Buffer *VEngine::rg::Registry::getBuffer(BufferViewHandle handle) const
{
	return m_graph.m_imageBuffers[(size_t)m_graph.m_viewDescriptions[(size_t)handle - 1].m_resourceHandle - 1].buffer;
}

BufferView *VEngine::rg::Registry::getBufferView(BufferViewHandle handle) const
{
	return m_graph.m_imageBufferViews[(size_t)handle - 1].bufferView;
}

VEngine::gal::DescriptorBufferInfo VEngine::rg::Registry::getBufferInfo(BufferViewHandle handle) const
{
	const size_t viewIdx = (size_t)handle - 1;
	const auto &viewDesc = m_graph.m_viewDescriptions[viewIdx];
	const size_t resIdx = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_graph.m_resourceDescriptions[resIdx];
	return { m_graph.m_imageBuffers[resIdx].buffer, resDesc.m_offset + viewDesc.m_offset, viewDesc.m_range, viewDesc.m_structureByteStride };
}

VEngine::rg::BufferViewDescription VEngine::rg::Registry::getBufferViewDescription(BufferViewHandle handle) const
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

VEngine::rg::BufferDescription VEngine::rg::Registry::getBufferDescription(BufferViewHandle handle) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	const auto &resDesc = m_graph.m_resourceDescriptions[(size_t)viewDesc.m_resourceHandle - 1];
	BufferDescription bufferDesc{};
	bufferDesc.m_name = resDesc.m_name;
	bufferDesc.m_clear = resDesc.m_clear;
	bufferDesc.m_clearValue = resDesc.m_clearValue;
	bufferDesc.m_size = resDesc.m_size;
	bufferDesc.m_usageFlags = resDesc.m_usageFlags;

	return bufferDesc;
}

bool VEngine::rg::Registry::firstUse(ResourceHandle handle) const
{
	return m_passHandleIndex == m_graph.m_resourceLifetimes[(size_t)handle - 1].first;
}

bool VEngine::rg::Registry::lastUse(ResourceHandle handle) const
{
	return m_passHandleIndex == m_graph.m_resourceLifetimes[(size_t)handle - 1].second;
}

void VEngine::rg::Registry::map(BufferViewHandle handle, void **data) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	const size_t resIdx = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_graph.m_resourceDescriptions[resIdx];
	assert(resDesc.m_hostVisible && !resDesc.m_external);
	m_graph.m_imageBuffers[resIdx].buffer->map(data);
	*data = ((uint8_t *)*data) + viewDesc.m_offset;
}

void VEngine::rg::Registry::unmap(BufferViewHandle handle) const
{
	const auto &viewDesc = m_graph.m_viewDescriptions[(size_t)handle - 1];
	const size_t resIdx = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_graph.m_resourceDescriptions[resIdx];
	assert(resDesc.m_hostVisible && !resDesc.m_external);
	m_graph.m_imageBuffers[resIdx].buffer->unmap();
}

VEngine::rg::RenderGraph::RenderGraph(gal::GraphicsDevice *graphicsDevice, Semaphore **semaphores, uint64_t *semaphoreValues)
	:m_graphicsDevice(graphicsDevice),
	m_semaphores(),
	m_semaphoreValues(),
	m_queues(),
	m_queueTimestampMasks(),
	m_recordTimings(true),
	m_timingInfos(std::make_unique<PassTimingInfo[]>(TIMESTAMP_QUERY_COUNT)),
	m_timingInfoCount(),
	m_commandListFramePool(m_graphicsDevice),
	m_queryPools(),
	m_finalWaitValues()
{
	m_queues[0] = m_graphicsDevice->getGraphicsQueue();
	m_queues[1] = m_graphicsDevice->getComputeQueue();
	m_queues[2] = m_graphicsDevice->getTransferQueue();

	for (size_t i = 0; i < 3; ++i)
	{
		m_semaphores[i] = semaphores[i];
		m_semaphoreValues[i] = semaphoreValues + i;
		m_graphicsDevice->createQueryPool(QueryType::TIMESTAMP, TIMESTAMP_QUERY_COUNT /*TODO: make this dynamic?*/, &m_queryPools[i]);

		const uint32_t validBits = m_queues[i]->getTimestampValidBits();
		m_queueTimestampMasks[i] = validBits >= 64 ? ~uint64_t() : ((uint64_t(1) << validBits) - 1);
	}

	gal::BufferCreateInfo createInfo{};
	createInfo.m_size = 3 * TIMESTAMP_QUERY_COUNT * 8;
	createInfo.m_createFlags = 0;
	createInfo.m_usageFlags = BufferUsageFlagBits::TRANSFER_DST_BIT;

	m_graphicsDevice->createBuffer(createInfo, MemoryPropertyFlagBits::HOST_CACHED_BIT | MemoryPropertyFlagBits::HOST_VISIBLE_BIT, 0, false, &m_queryResultBuffer);
}

VEngine::rg::RenderGraph::~RenderGraph()
{
	reset();
	for (size_t i = 0; i < 3; ++i)
	{
		m_graphicsDevice->destroyQueryPool(m_queryPools[i]);
	}
	m_graphicsDevice->destroyBuffer(m_queryResultBuffer);
}

void VEngine::rg::RenderGraph::addPass(const char *name, QueueType queueType, uint32_t passResourceUsageCount, const ResourceUsageDescription *passResourceUsages, const RecordFunc &recordFunc, bool forceExecution)
{
#ifdef _DEBUG
	for (uint32_t i = 0; i < passResourceUsageCount; ++i)
	{
		// catches most false api usages
		assert(passResourceUsages[i].m_viewHandle);
		// not a very good test, but if the handle has a higher value than the number of views, something must be off
		assert(size_t(passResourceUsages[i].m_viewHandle) <= m_viewDescriptions.size());
	}
#endif // _DEBUG

	const uint16_t passIndex = static_cast<uint16_t>(m_passSubresources.size());

	m_passNames.push_back(name);
	m_passRecordInfo.push_back({ recordFunc, getQueue(queueType) });

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

		const auto viewSubresourceCount = viewDesc.m_image ? viewDesc.m_subresourceRange.m_layerCount * viewDesc.m_subresourceRange.m_levelCount : 1;
		passSubresources.m_readSubresourceCount += Initializers::isReadAccess(usage.m_stateStageMask.m_resourceState) ? viewSubresourceCount : 0;
		passSubresources.m_writeSubresourceCount += Initializers::isWriteAccess(usage.m_stateStageMask.m_resourceState) ? viewSubresourceCount : 0;

		const ResourceUsage resUsage{ passIndex, usage.m_stateStageMask, usage.m_customUsage ? usage.m_finalStateStageMask : usage.m_stateStageMask };
		forEachSubresource(usage.m_viewHandle, [&](uint32_t index) {m_resourceUsages[index].push_back(resUsage); });
	}

	m_passSubresourceIndices.reserve(m_passSubresourceIndices.size() + passSubresources.m_readSubresourceCount + passSubresources.m_writeSubresourceCount);

	// add read resource indices to list and update resource usages
	for (uint32_t i = 0; i < passResourceUsageCount; ++i)
	{
		const auto &usage = passResourceUsages[i];
		const bool readAccess = Initializers::isReadAccess(usage.m_stateStageMask.m_resourceState);
		if (readAccess)
		{
			forEachSubresource(usage.m_viewHandle, [&](uint32_t index) { m_passSubresourceIndices.push_back(static_cast<uint16_t>(index)); });
		}
	}
	// add write resources
	for (uint32_t i = 0; i < passResourceUsageCount; ++i)
	{
		const auto &usage = passResourceUsages[i];
		const bool writeAccess = Initializers::isWriteAccess(usage.m_stateStageMask.m_resourceState);
		if (writeAccess)
		{
			forEachSubresource(usage.m_viewHandle, [&](uint32_t index) { m_passSubresourceIndices.push_back(static_cast<uint16_t>(index)); });
		}
	}
}

VEngine::rg::ImageViewHandle VEngine::rg::RenderGraph::createImageView(const ImageViewDescription &viewDesc)
{
	const auto &resDesc = m_resourceDescriptions[(size_t)viewDesc.m_imageHandle - 1];
	ResourceViewDescription desc{};
	desc.m_name = viewDesc.m_name;
	desc.m_resourceHandle = ResourceHandle(viewDesc.m_imageHandle);
	desc.m_viewType = viewDesc.m_viewType;
	desc.m_format = viewDesc.m_format == Format::UNDEFINED ? resDesc.m_format : viewDesc.m_format;
	desc.m_components = viewDesc.m_components;
	desc.m_subresourceRange = viewDesc.m_subresourceRange;
	desc.m_image = true;

	m_viewDescriptions.push_back(desc);

	return ImageViewHandle(m_viewDescriptions.size());
}

VEngine::rg::BufferViewHandle VEngine::rg::RenderGraph::createBufferView(const BufferViewDescription &viewDesc)
{
	ResourceViewDescription desc{};
	desc.m_name = viewDesc.m_name;
	desc.m_resourceHandle = ResourceHandle(viewDesc.m_bufferHandle);
	desc.m_format = viewDesc.m_format;
	desc.m_offset = viewDesc.m_offset;
	desc.m_range = viewDesc.m_range;
	desc.m_structureByteStride = viewDesc.m_structureByteStride;

	m_viewDescriptions.push_back(desc);

	return BufferViewHandle(m_viewDescriptions.size());
}

VEngine::rg::ImageHandle VEngine::rg::RenderGraph::createImage(const ImageDescription &imageDesc)
{
	ResourceDescription resDesc{};
	resDesc.m_name = imageDesc.m_name;
	resDesc.m_usageFlags = imageDesc.m_usageFlags | (imageDesc.m_clear ? ImageUsageFlagBits::TRANSFER_DST_BIT : 0);
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
	resDesc.m_concurrent = false;
	resDesc.m_image = true;

	assert(resDesc.m_width && resDesc.m_height && resDesc.m_layers && resDesc.m_levels);

	m_resourceDescriptions.push_back(resDesc);
	m_resourceUsageOffsets.push_back(static_cast<uint32_t>(m_resourceUsages.size()));
	for (size_t i = 0; i < resDesc.m_subresourceCount; ++i)
	{
		m_resourceUsages.push_back({});
	}

	return ImageHandle(m_resourceDescriptions.size());
}

VEngine::rg::BufferHandle VEngine::rg::RenderGraph::createBuffer(const BufferDescription &bufferDesc)
{
	ResourceDescription resDesc{};
	resDesc.m_name = bufferDesc.m_name;
	resDesc.m_usageFlags = bufferDesc.m_usageFlags | (bufferDesc.m_clear ? BufferUsageFlagBits::TRANSFER_DST_BIT : 0);
	resDesc.m_clear = bufferDesc.m_clear;
	resDesc.m_clearValue = bufferDesc.m_clearValue;
	resDesc.m_offset = 0;
	resDesc.m_size = bufferDesc.m_size;
	resDesc.m_subresourceCount = 1;
	resDesc.m_concurrent = true;
	resDesc.m_hostVisible = bufferDesc.m_hostVisible;

	assert(resDesc.m_size);

	m_resourceDescriptions.push_back(resDesc);
	m_resourceUsageOffsets.push_back(static_cast<uint32_t>(m_resourceUsages.size()));
	m_resourceUsages.push_back({});

	return BufferHandle(m_resourceDescriptions.size());
}

VEngine::rg::ImageHandle VEngine::rg::RenderGraph::importImage(Image *image, const char *name, bool clear, const ClearValue &clearValue, ResourceStateData *resourceStateData)
{
	const auto &imageDesc = image->getDescription();

	ResourceDescription resDesc{};
	resDesc.m_name = name;
	resDesc.m_usageFlags = imageDesc.m_usageFlags;
	resDesc.m_clear = clear;
	resDesc.m_clearValue = clearValue;
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
	resDesc.m_concurrent = false;
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

	m_externalResourceInfo.push_back(resourceStateData);

	m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE, image, name);

	return ImageHandle(m_resourceDescriptions.size());
}

VEngine::rg::BufferHandle VEngine::rg::RenderGraph::importBuffer(Buffer *buffer, const char *name, bool clear, const ClearValue &clearValue, ResourceStateData *resourceStateData)
{
	const auto &bufferDesc = buffer->getDescription();

	ResourceDescription resDesc{};
	resDesc.m_name = name;
	resDesc.m_usageFlags = bufferDesc.m_usageFlags;
	resDesc.m_clear = clear;
	resDesc.m_clearValue = clearValue;
	resDesc.m_offset = 0; // TODO
	resDesc.m_size = bufferDesc.m_size;
	resDesc.m_subresourceCount = 1;
	resDesc.m_externalInfoIndex = static_cast<uint16_t>(m_externalResourceInfo.size());
	resDesc.m_concurrent = true;
	resDesc.m_external = true;

	m_resourceDescriptions.push_back(resDesc);
	m_resourceUsageOffsets.push_back(static_cast<uint32_t>(m_resourceUsages.size()));
	m_resourceUsages.push_back({});

	m_imageBuffers.resize(m_resourceDescriptions.size());
	m_imageBuffers.back().buffer = buffer;

	m_externalResourceInfo.push_back(resourceStateData);

	m_graphicsDevice->setDebugObjectName(ObjectType::BUFFER, buffer, name);

	return BufferHandle(m_resourceDescriptions.size());
}

void VEngine::rg::RenderGraph::reset()
{
	// dont try to reset if already in ready-to-record state
	if (m_readyToRecord)
	{
		return;
	}

	// wait on semaphores for queue completion
	for (size_t i = 0; i < 3; ++i)
	{
		m_semaphores[i]->wait(m_finalWaitValues[i]);
	}

	// destroy internal resources
	for (size_t i = 0; i < m_resourceDescriptions.size(); ++i)
	{
		const auto &resDesc = m_resourceDescriptions[i];
		if (m_culledResources[i] && !resDesc.m_external)
		{
			if (resDesc.m_image)
			{
				m_graphicsDevice->destroyImage(m_imageBuffers[i].image);
			}
			else
			{
				m_graphicsDevice->destroyBuffer(m_imageBuffers[i].buffer);
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
				m_graphicsDevice->destroyImageView(m_imageBufferViews[i].imageView);
			}
			else if (m_imageBufferViews[i].bufferView != nullptr)
			{
				m_graphicsDevice->destroyBufferView(m_imageBufferViews[i].bufferView);
			}
		}
	}

	if (m_recordTimings && !m_passHandleOrder.empty())
	{
		m_timingInfoCount = static_cast<uint32_t>(m_passHandleOrder.size());

		uint64_t *queryResultData;
		m_queryResultBuffer->map((void **)&queryResultData);
		MemoryRange range{ 0, TIMESTAMP_QUERY_COUNT * 3 * 8 };
		m_queryResultBuffer->invalidate(1, &range);

		uint32_t queryOffsets[3] = {};
		for (uint32_t i = 0; i < m_passHandleOrder.size(); ++i)
		{
			const Queue *queue = m_passRecordInfo[m_passHandleOrder[i]].m_queue;
			const uint32_t queueIndex = static_cast<uint32_t>(queue->getQueueType());

			// zero initialize data so that passes on queues without timestamp support will have timings of 0
			uint64_t data[4]{};
			if (m_queueTimestampMasks[queueIndex])
			{
				//m_graphicsDevice->getQueryPoolResults(m_queryPools[queueIndex], i * 4, 4, sizeof(data), data, sizeof(data[0]), QueryResultFlagBits::_64_BIT | QueryResultFlagBits::WAIT_BIT);

				// mask value by valid timestamp bits
				for (size_t j = 0; j < 4; ++j)
				{
					data[j] = queryResultData[queueIndex * TIMESTAMP_QUERY_COUNT + queryOffsets[queueIndex] + j];
					data[j] = data[j] & m_queueTimestampMasks[queueIndex];
				}

				queryOffsets[queueIndex] += 4;
			}

			const float timestampPeriod = queue->getTimestampPeriod();

			m_timingInfos[i] =
			{
				m_passNames[m_passHandleOrder[i]],
				static_cast<float>((data[2] - data[1]) * (timestampPeriod * (1.0 / 1e6))),
				static_cast<float>((data[3] - data[0]) * (timestampPeriod * (1.0 / 1e6))),
			};
		}
		m_queryResultBuffer->unmap();
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
		m_imageBuffers.clear();
		m_imageBufferViews.clear();
		m_passHandleOrder.clear();
		m_commandLists.clear();
		m_batches.clear();
		m_passRecordInfo.clear();
		for (size_t i = 0; i < 3; ++i)
		{
			m_externalReleaseBarriers[i].clear();
		}
	}

	m_commandListFramePool.reset();
	
	m_readyToRecord = true;
}

void VEngine::rg::RenderGraph::execute(ResourceViewHandle finalResourceHandle, const ResourceStateData &finalResourceStateData, bool forceWaitOnSemaphore, uint64_t waitValue)
{
	createPasses(finalResourceHandle, finalResourceStateData);
	createResources();
	createSynchronization(finalResourceHandle, forceWaitOnSemaphore, waitValue);
	record();
	m_readyToRecord = false;
}

void VEngine::rg::RenderGraph::getTimingInfo(size_t *count, const PassTimingInfo **data) const
{
	*count = m_timingInfoCount;
	*data = m_timingInfos.get();
}

Queue *VEngine::rg::RenderGraph::getQueue(QueueType queueType) const
{
	return m_queues[static_cast<size_t>(queueType)];
}

void VEngine::rg::RenderGraph::forEachSubresource(ResourceViewHandle handle, std::function<void(uint32_t)> func)
{
	const size_t viewIndex = (size_t)handle - 1;
	const auto &viewDesc = m_viewDescriptions[viewIndex];
	const size_t resIndex = (size_t)viewDesc.m_resourceHandle - 1;
	const auto &resDesc = m_resourceDescriptions[resIndex];
	const uint32_t resourceUsagesOffset = m_resourceUsageOffsets[resIndex];

	if (resDesc.m_image)
	{
		const uint32_t baseLayer = viewDesc.m_subresourceRange.m_baseArrayLayer;
		const uint32_t layerCount = viewDesc.m_subresourceRange.m_layerCount;
		const uint32_t baseLevel = viewDesc.m_subresourceRange.m_baseMipLevel;
		const uint32_t levelCount = viewDesc.m_subresourceRange.m_levelCount;
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

void VEngine::rg::RenderGraph::createPasses(ResourceViewHandle finalResourceHandle, const ResourceStateData &finalResourceState)
{
	// add pass to transition final resource
	if (finalResourceState.m_stateStageMask.m_resourceState != ResourceState::UNDEFINED)
	{
		ResourceUsageDescription resourceUsageDesc{ finalResourceHandle, finalResourceState.m_stateStageMask };
		size_t queueIdx = finalResourceState.m_queue == m_queues[0] ? 0 : finalResourceState.m_queue == m_queues[1] ? 1 : 2;
		addPass("Final Resource Transition", static_cast<QueueType>(queueIdx), 1, &resourceUsageDesc, [](CommandList *, const Registry &) {}, true);
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
				const bool readAccess = Initializers::isReadAccess(m_resourceUsages[subresourceIdx][usageIdx].m_initialResourceState.m_resourceState);
				if (readAccess)
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
				const bool writeAccess = Initializers::isWriteAccess(usage.m_initialResourceState.m_resourceState);
				if (writeAccess && passRefCounts[passHandle] != 0 && --passRefCounts[passHandle] == 0)
				{
					--survivingPassCount;
					const uint32_t start = m_passSubresources[passHandle].m_subresourcesOffset;
					const uint32_t end = m_passSubresources[passHandle].m_subresourcesOffset + m_passSubresources[passHandle].m_readSubresourceCount;
					for (uint32_t i = start; i < end; ++i)
					{
						auto &refCount = subresourceRefCounts[m_passSubresourceIndices[i]];
						if (refCount != 0 && --refCount == 0)
						{
							resourceStack.push(m_passSubresourceIndices[i]);
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
							const auto state = resDesc.m_image ?
								ResourceStateStageMask{ ResourceState::CLEAR_IMAGE, PipelineStageFlagBits::CLEAR_BIT } :
								ResourceStateStageMask{ ResourceState::CLEAR_BUFFER, PipelineStageFlagBits::CLEAR_BIT };
							m_resourceUsages[subresourceIdx].insert(m_resourceUsages[subresourceIdx].cbegin(), { clearPassHandle, state, state });
							m_passSubresourceIndices.push_back(subresourceIdx);
						}
						passSubresource.m_writeSubresourceCount += subresourceCount;
						m_resourceLifetimes[resourceIdx].first = clearPassHandle;
					}

					// create record func and set appropriate queue
					m_passRecordInfo.push_back({ [this, clrResources{std::move(clearResources[i])}] (CommandList *cmdList, const Registry &registry)
					{
						for (const auto resourceIdx : clrResources)
						{
							const auto &resDesc = m_resourceDescriptions[resourceIdx];
							if (resDesc.m_image)
							{
								Image *image = registry.getImage(ImageHandle((size_t)resourceIdx + 1));
								const auto &imageDesc = image->getDescription();
								const ImageSubresourceRange subresourceRange{ 0, imageDesc.m_levels, 0, imageDesc.m_layers };
								if (Initializers::isDepthFormat(resDesc.m_format))
								{
									cmdList->clearDepthStencilImage(image, &resDesc.m_clearValue.m_imageClearValue.m_depthStencil, 1, &subresourceRange);
								}
								else
								{
									cmdList->clearColorImage(image, &resDesc.m_clearValue.m_imageClearValue.m_color, 1, &subresourceRange);
								}
							}
							else
							{
								cmdList->fillBuffer(registry.getBuffer(BufferHandle((size_t)resourceIdx + 1)), 0, resDesc.m_size, resDesc.m_clearValue.m_bufferClearValue);
							}
						}
					}, m_passRecordInfo[i].m_queue });
					m_viewUsages.push_back({});
					handleToIndex.push_back(static_cast<uint16_t>(m_passHandleOrder.size()));
					m_passHandleOrder.push_back(clearPassHandle);
				}
				handleToIndex[i] = static_cast<uint16_t>(m_passHandleOrder.size());
				m_passHandleOrder.push_back(i);
				m_passRecordInfo[i].m_signalValue = handleToIndex[i];
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
							resDesc.m_imageFlags |= ImageCreateFlagBits::MUTABLE_FORMAT_BIT;
						}
						if (viewDesc.m_viewType == ImageViewType::CUBE || viewDesc.m_viewType == ImageViewType::CUBE_ARRAY)
						{
							resDesc.m_imageFlags |= ImageCreateFlagBits::CUBE_COMPATIBLE_BIT;
						}
						else if (resDesc.m_imageType == ImageType::_3D && viewDesc.m_viewType == ImageViewType::_2D_ARRAY)
						{
							resDesc.m_imageFlags |= ImageCreateFlagBits::_2D_ARRAY_COMPATIBLE_BIT;
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

void VEngine::rg::RenderGraph::createResources()
{
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

		uint32_t usageFlags = resDesc.m_usageFlags;

		// find usage flags and refCount
		const uint32_t subresourcesEndIndex = resDesc.m_subresourceCount + m_resourceUsageOffsets[resourceIndex];
		for (uint32_t i = m_resourceUsageOffsets[resourceIndex]; i < subresourcesEndIndex; ++i)
		{
			for (const auto &usage : m_resourceUsages[i])
			{
				usageFlags |= Initializers::getUsageFlags(usage.m_initialResourceState.m_resourceState);
				usageFlags |= Initializers::getUsageFlags(usage.m_finalResourceState.m_resourceState);
			}
		}

		// is resource image or buffer?
		if (resDesc.m_image)
		{
			// create image
			ImageCreateInfo imageCreateInfo{};
			imageCreateInfo.m_width = resDesc.m_width;
			imageCreateInfo.m_height = resDesc.m_height;
			imageCreateInfo.m_depth = resDesc.m_depth;
			imageCreateInfo.m_levels = resDesc.m_levels;
			imageCreateInfo.m_layers = resDesc.m_layers;
			imageCreateInfo.m_samples = resDesc.m_samples;
			imageCreateInfo.m_imageType = resDesc.m_imageType;
			imageCreateInfo.m_format = resDesc.m_format;
			imageCreateInfo.m_createFlags = resDesc.m_imageFlags;
			imageCreateInfo.m_usageFlags = usageFlags;

			m_graphicsDevice->createImage(imageCreateInfo, MemoryPropertyFlagBits::DEVICE_LOCAL_BIT, 0, false, &m_imageBuffers[resourceIndex].image);
		}
		else
		{
			// create buffer
			BufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.m_size = resDesc.m_size;
			bufferCreateInfo.m_createFlags = 0;
			bufferCreateInfo.m_usageFlags = usageFlags;

			auto requiredFlags = resDesc.m_hostVisible ? MemoryPropertyFlagBits::HOST_VISIBLE_BIT | MemoryPropertyFlagBits::HOST_COHERENT_BIT : MemoryPropertyFlagBits::DEVICE_LOCAL_BIT;
			auto preferredFlags = resDesc.m_hostVisible ? MemoryPropertyFlagBits::DEVICE_LOCAL_BIT : 0;

			m_graphicsDevice->createBuffer(bufferCreateInfo, requiredFlags, preferredFlags, false, &m_imageBuffers[resourceIndex].buffer);
		}

		m_graphicsDevice->setDebugObjectName(resDesc.m_image ? ObjectType::IMAGE : ObjectType::BUFFER, m_imageBuffers[resourceIndex].image, resDesc.m_name);
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
			ImageViewCreateInfo viewCreateInfo{};
			viewCreateInfo.m_image = m_imageBuffers[(size_t)viewDesc.m_resourceHandle - 1].image;
			viewCreateInfo.m_viewType = viewDesc.m_viewType;
			viewCreateInfo.m_format = viewDesc.m_format;
			viewCreateInfo.m_components = viewDesc.m_components;
			viewCreateInfo.m_baseMipLevel = viewDesc.m_subresourceRange.m_baseMipLevel;
			viewCreateInfo.m_levelCount = viewDesc.m_subresourceRange.m_levelCount;
			viewCreateInfo.m_baseArrayLayer = viewDesc.m_subresourceRange.m_baseArrayLayer;
			viewCreateInfo.m_layerCount = viewDesc.m_subresourceRange.m_layerCount;

			m_graphicsDevice->createImageView(viewCreateInfo, &m_imageBufferViews[viewIndex].imageView);

			m_graphicsDevice->setDebugObjectName(ObjectType::IMAGE_VIEW, m_imageBufferViews[viewIndex].imageView, viewDesc.m_name);
		}
		else if (viewDesc.m_format != Format::UNDEFINED)
		{
			BufferViewCreateInfo viewCreateInfo{};
			viewCreateInfo.m_buffer = m_imageBuffers[(size_t)viewDesc.m_resourceHandle - 1].buffer;
			viewCreateInfo.m_format = viewDesc.m_format;
			viewCreateInfo.m_offset = viewDesc.m_offset;
			viewCreateInfo.m_range = viewDesc.m_range;

			m_graphicsDevice->createBufferView(viewCreateInfo, &m_imageBufferViews[viewIndex].bufferView);

			m_graphicsDevice->setDebugObjectName(ObjectType::BUFFER_VIEW, m_imageBufferViews[viewIndex].bufferView, viewDesc.m_name);
		}
	}
}

void VEngine::rg::RenderGraph::createSynchronization(ResourceViewHandle finalResourceHandle, bool forceWaitOnSemaphore, uint64_t forceWaitValue)
{
	struct SemaphoreDependencyInfo
	{
		PipelineStageFlags m_waitDstStageMasks[3] = {};
		uint64_t m_waitValues[3] = {};
	};

	std::vector<SemaphoreDependencyInfo> semaphoreDependencies(m_passSubresources.size());

	const uint32_t finalResourceIdx = (uint32_t)m_viewDescriptions[(size_t)finalResourceHandle - 1].m_resourceHandle - 1;

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
				Queue *m_queue;
				ResourceStateStageMask m_stateStageMask;
			};

			Queue *firstUsageQueue = m_passRecordInfo[m_resourceUsages[subresourceIdx][0].m_passHandle].m_queue;
			const uint32_t relativeSubresIdx = subresourceIdx - usageOffset;
			UsageInfo prevUsageInfo{ 0, firstUsageQueue, ResourceStateStageMask{ResourceState::UNDEFINED, PipelineStageFlagBits::TOP_OF_PIPE_BIT} };

			// if the resource is external, change prevUsageInfo accordingly and update external info values
			if (resDesc.m_external)
			{
				const auto &extInfo = m_externalResourceInfo[resDesc.m_externalInfoIndex];
				prevUsageInfo.m_queue = extInfo && extInfo[relativeSubresIdx].m_queue ? extInfo[relativeSubresIdx].m_queue : prevUsageInfo.m_queue;
				prevUsageInfo.m_stateStageMask = extInfo ? extInfo[relativeSubresIdx].m_stateStageMask : prevUsageInfo.m_stateStageMask;
				prevUsageInfo.m_passHandle = prevUsageInfo.m_queue == m_queues[0] ? 0 : prevUsageInfo.m_queue == m_queues[1] ? 1 : 2;

				// update external info values
				if (extInfo)
				{
					const auto &lastUsage = m_resourceUsages[subresourceIdx].back();
					extInfo[relativeSubresIdx].m_queue = m_passRecordInfo[lastUsage.m_passHandle].m_queue;
					extInfo[relativeSubresIdx].m_stateStageMask = lastUsage.m_finalResourceState;
				}
			}

			const size_t usageCount = m_resourceUsages[subresourceIdx].size();
			for (size_t usageIdx = 0; usageIdx < usageCount; ++usageIdx)
			{
				const auto &subresUsage = m_resourceUsages[subresourceIdx][usageIdx];
				auto &passRecordInfo = m_passRecordInfo[subresUsage.m_passHandle];

				UsageInfo curUsageInfo{ subresUsage.m_passHandle, passRecordInfo.m_queue, subresUsage.m_initialResourceState };

				Barrier barrier{};
				barrier.m_image = resDesc.m_image ? m_imageBuffers[resourceIdx].image : nullptr;
				barrier.m_buffer = !resDesc.m_image ? m_imageBuffers[resourceIdx].buffer : nullptr;
				barrier.m_stagesBefore = prevUsageInfo.m_stateStageMask.m_stageMask;
				barrier.m_stagesAfter = curUsageInfo.m_stateStageMask.m_stageMask;
				barrier.m_stateBefore = prevUsageInfo.m_stateStageMask.m_resourceState;
				barrier.m_stateAfter = curUsageInfo.m_stateStageMask.m_resourceState;
				barrier.m_srcQueue = prevUsageInfo.m_queue;
				barrier.m_dstQueue = curUsageInfo.m_queue;
				barrier.m_imageSubresourceRange = { relativeSubresIdx % resDesc.m_levels, 1, relativeSubresIdx / resDesc.m_levels, 1 };
				barrier.m_queueOwnershipAcquireBarrier = barrier.m_srcQueue != barrier.m_dstQueue && !resDesc.m_concurrent;
				barrier.m_queueOwnershipReleaseBarrier = false;

				passRecordInfo.m_beforeBarriers.push_back(barrier);

				const size_t prevQueueIdx = prevUsageInfo.m_queue == m_queues[0] ? 0 : prevUsageInfo.m_queue == m_queues[1] ? 1 : 2;

				// add optional forced wait on semaphore for final resource
				if (forceWaitOnSemaphore && resourceIdx == finalResourceIdx && m_passHandleOrder[m_resourceLifetimes[resourceIdx].first] == subresUsage.m_passHandle)
				{
					semaphoreDependencies[curUsageInfo.m_passHandle].m_waitDstStageMasks[prevQueueIdx] |= curUsageInfo.m_stateStageMask.m_stageMask;
					auto &waitValue = semaphoreDependencies[curUsageInfo.m_passHandle].m_waitValues[prevQueueIdx];
					waitValue = std::max(forceWaitValue, waitValue);
				}

				// we just acquired ownership of the resource -> add a release barrier on the previous queue
				if (barrier.m_queueOwnershipAcquireBarrier)
				{
					barrier.m_queueOwnershipAcquireBarrier = false;
					barrier.m_queueOwnershipReleaseBarrier = true;

					semaphoreDependencies[curUsageInfo.m_passHandle].m_waitDstStageMasks[prevQueueIdx] |= curUsageInfo.m_stateStageMask.m_stageMask;
					auto &waitValue = semaphoreDependencies[curUsageInfo.m_passHandle].m_waitValues[prevQueueIdx];

					// external dependency
					if (usageIdx == 0)
					{
						m_externalReleaseBarriers[prevUsageInfo.m_passHandle].push_back(barrier);
						waitValue = std::max(*m_semaphoreValues[prevQueueIdx] + 1, waitValue);
					}
					else
					{
						auto &prevPassRecordInfo = m_passRecordInfo[prevUsageInfo.m_passHandle];
						prevPassRecordInfo.m_afterBarriers.push_back(barrier);
						waitValue = std::max(*m_semaphoreValues[prevQueueIdx] + 1 + m_passRecordInfo[prevUsageInfo.m_passHandle].m_signalValue, waitValue);
					}
				}

				// update prevUsageInfo
				prevUsageInfo = curUsageInfo;
				prevUsageInfo.m_stateStageMask = subresUsage.m_finalResourceState;
			}
		}
	}

	// create batches
	Queue *prevQueue = nullptr;
	bool startNewBatch = true;
	for (uint16_t i = 0; i < m_passHandleOrder.size(); ++i)
	{
		const uint16_t passHandle = m_passHandleOrder[i];
		const auto &semaphoreDependency = semaphoreDependencies[passHandle];
		Queue *curQueue = m_passRecordInfo[passHandle].m_queue;

		// if the previous pass needs to signal, startNewBatch is already true
		// if the queue type changed, we need to start a new batch
		// if this pass needs to wait, we also need a need batch
		startNewBatch = startNewBatch
			|| prevQueue != curQueue
			|| semaphoreDependency.m_waitDstStageMasks[0]
			|| semaphoreDependency.m_waitDstStageMasks[1]
			|| semaphoreDependency.m_waitDstStageMasks[2];
		if (startNewBatch)
		{
			startNewBatch = false;
			m_batches.push_back({});
			auto &batch = m_batches.back();
			batch.m_passIndexOffset = i;
			batch.m_cmdListOffset = 3 + m_batches.size() - 1;
			batch.m_queue = curQueue;
			for (size_t j = 0; j < 3; ++j)
			{
				batch.m_waitDstStageMasks[j] = semaphoreDependency.m_waitDstStageMasks[j];
				batch.m_waitValues[j] = semaphoreDependency.m_waitValues[j];
			}
		}

		// some other passes needs to wait on this one or this is the last pass -> end batch after this pass
		if (!m_passRecordInfo[passHandle].m_afterBarriers.empty() || i == m_passHandleOrder.size() - 1)
		{
			startNewBatch = true;
		}

		// update signal value to current last pass in batch
		auto &batch = m_batches.back();
		const size_t queueIdx = curQueue == m_queues[0] ? 0 : curQueue == m_queues[1] ? 1 : 2;
		batch.m_signalValue = std::max(*m_semaphoreValues[queueIdx] + 1 + m_passRecordInfo[passHandle].m_signalValue, batch.m_signalValue);

		prevQueue = curQueue;
		++batch.m_passIndexCount;
	}

	// walk backwards over batches and find last batch for each queue
	bool foundLastBatch[3] = {};
	for (size_t i = m_batches.size(); i > 0; --i)
	{
		auto &batch = m_batches[i - 1];
		const size_t queueIdx = batch.m_queue == m_queues[0] ? 0 : batch.m_queue == m_queues[1] ? 1 : 2;
		if (!foundLastBatch[queueIdx])
		{
			foundLastBatch[queueIdx] = true;
			batch.m_lastBatchOnQueue = true;

			if (foundLastBatch[0] && foundLastBatch[1] && foundLastBatch[2])
			{
				break;
			}
		}
	}
}

void VEngine::rg::RenderGraph::record()
{
	m_commandLists.resize(m_batches.size() + 3);

	// release passes for external resources
	const char *releasePassNames[]
	{
		"release barriers for external resources (graphics queue)",
		"release barriers for external resources (compute queue)",
		"release barriers for external resources (transfer queue)",
	};
	for (size_t i = 0; i < 3; ++i)
	{
		if (!m_externalReleaseBarriers[i].empty())
		{
			CommandList *cmdList = m_commandLists[i] = m_commandListFramePool.acquire(m_queues[i]);

			cmdList->begin();

			cmdList->insertDebugLabel(releasePassNames[i]);
			cmdList->barrier(static_cast<uint32_t>(m_externalReleaseBarriers[i].size()), m_externalReleaseBarriers[i].data());

			cmdList->end();
		}
	}

	// each pass uses 4 queries, so make sure we dont exceed the query pool size
	assert(!m_recordTimings || m_passHandleOrder.size() <= (TIMESTAMP_QUERY_COUNT / 4));

	bool resetQueryPools[3] = {};
	uint32_t queryCounts[3] = {};

	// record passes
	for (const auto &batch : m_batches)
	{
		const uint32_t queueTypeIndex = batch.m_queue == m_queues[0] ? 0 : batch.m_queue == m_queues[1] ? 1 : 2;
		const bool recordTimings = m_recordTimings && m_queueTimestampMasks[queueTypeIndex];

		// get command list
		CommandList *cmdList = m_commandLists[batch.m_cmdListOffset] = m_commandListFramePool.acquire(batch.m_queue);

		// start recording
		cmdList->begin();

		for (uint16_t i = 0; i < batch.m_passIndexCount; ++i)
		{
			const uint16_t passHandle = m_passHandleOrder[i + batch.m_passIndexOffset];
			const auto &pass = m_passRecordInfo[passHandle];

			cmdList->beginDebugLabel(m_passNames[passHandle]);

			const uint32_t queryIndex = queryCounts[queueTypeIndex];// (i + batch.m_passIndexOffset) * 4;
			
			// disable timestamps if the queue doesnt support it
			if (recordTimings && !resetQueryPools[queueTypeIndex])
			{
				cmdList->resetQueryPool(m_queryPools[queueTypeIndex], 0, TIMESTAMP_QUERY_COUNT);
				resetQueryPools[queueTypeIndex] = true;
			}

			// write timestamp before waits
			if (recordTimings)
			{
				cmdList->writeTimestamp(PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT, m_queryPools[queueTypeIndex], queryIndex + 0);
			}

			// pipeline barrier
			if (!pass.m_beforeBarriers.empty())
			{
				cmdList->barrier(static_cast<uint32_t>(pass.m_beforeBarriers.size()), pass.m_beforeBarriers.data());
			}

			// write timestamp before pass commands
			if (recordTimings)
			{
				cmdList->writeTimestamp(PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT, m_queryPools[queueTypeIndex], queryIndex + 1);
			}

			// record commands
			pass.m_recordFunc(cmdList, Registry(*this, i));

			// write timestamp after pass commands
			if (recordTimings)
			{
				cmdList->writeTimestamp(PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT, m_queryPools[queueTypeIndex], queryIndex + 2);
			}

			// release resources
			if (!pass.m_afterBarriers.empty())
			{
				cmdList->barrier(static_cast<uint32_t>(pass.m_afterBarriers.size()), pass.m_afterBarriers.data());
			}

			// write timestamp after all commands
			if (recordTimings)
			{
				cmdList->writeTimestamp(PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT, m_queryPools[queueTypeIndex], queryIndex + 3);
				queryCounts[queueTypeIndex] += 4;
			}

			cmdList->endDebugLabel();
		}

		if (recordTimings && batch.m_lastBatchOnQueue)
		{
			// 4 queries per pass and 8 byte per query
			cmdList->copyQueryPoolResults(m_queryPools[queueTypeIndex], 0, queryCounts[queueTypeIndex], m_queryResultBuffer, queueTypeIndex * TIMESTAMP_QUERY_COUNT * 8);
		}

		// end recording
		cmdList->end();
	}

	// insert external resource release batches
	{
		Batch releaseBatches[3]{};
		size_t batchCount = 0;
		for (size_t i = 0; i < 3; ++i)
		{
			if (!m_externalReleaseBarriers[i].empty())
			{
				releaseBatches[batchCount].m_passIndexCount = 1;
				releaseBatches[batchCount].m_cmdListOffset = static_cast<uint16_t>(i);
				releaseBatches[batchCount].m_queue = m_queues[i];
				releaseBatches[batchCount].m_waitDstStageMasks[i] = PipelineStageFlagBits::TOP_OF_PIPE_BIT;
				releaseBatches[batchCount].m_waitValues[i] = *m_semaphoreValues[i];
				releaseBatches[batchCount].m_signalValue = *m_semaphoreValues[i] + 1;
				++batchCount;
			}
		}
		m_batches.insert(m_batches.begin(), releaseBatches, releaseBatches + batchCount);
	}

	// submit all batches
	for (const auto &batch : m_batches)
	{
		gal::Semaphore *semaphores[3];
		uint32_t waitDstStageMasks[3];
		uint64_t waitValues[3];
		uint32_t waitCount = 0;
		for (size_t i = 0; i < 3; ++i)
		{
			if (batch.m_waitDstStageMasks[i] != 0)
			{
				semaphores[waitCount] = m_semaphores[i];
				waitDstStageMasks[waitCount] = batch.m_waitDstStageMasks[i];
				waitValues[waitCount] = batch.m_waitValues[i];
				++waitCount;
			}
		}

		uint32_t queueIdx = batch.m_queue == m_queues[0] ? 0 : batch.m_queue == m_queues[1] ? 1 : 2;

		SubmitInfo submitInfo;
		submitInfo.m_waitSemaphoreCount = waitCount;
		submitInfo.m_waitSemaphores = semaphores;
		submitInfo.m_waitValues = waitValues;
		submitInfo.m_waitDstStageMask = waitDstStageMasks;
		submitInfo.m_commandListCount = 1;// batch.m_passIndexCount;
		submitInfo.m_commandLists = m_commandLists.data() + batch.m_cmdListOffset;
		submitInfo.m_signalSemaphoreCount = 1;
		submitInfo.m_signalSemaphores = &m_semaphores[queueIdx];
		submitInfo.m_signalValues = &batch.m_signalValue;

		batch.m_queue->submit(1, &submitInfo);

		m_finalWaitValues[queueIdx] = std::max(m_finalWaitValues[queueIdx], batch.m_signalValue);
	}

	*m_semaphoreValues[0] = m_finalWaitValues[0];
	*m_semaphoreValues[1] = m_finalWaitValues[1];
	*m_semaphoreValues[2] = m_finalWaitValues[2];
}
