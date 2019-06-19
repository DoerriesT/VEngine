#include "VKDescriptorSetCache.h"
#include "VKContext.h"
#include "Utility/Utility.h"

VkDescriptorSet VEngine::VKDescriptorSetCache::getDescriptorSet(VkDescriptorSetLayout layout, uint32_t *typeCounts)
{
	auto &pooledSetsPtr = m_layoutToSets[layout];

	// layout doesnt exist yet -> create it
	if (!pooledSetsPtr)
	{
		pooledSetsPtr = std::make_unique<DescriptorSetPools>(layout, typeCounts);
	}

	return pooledSetsPtr->getDescriptorSet(m_frameIndex);
}

void VEngine::VKDescriptorSetCache::update(size_t currentFrameIndex, size_t frameIndexToRelease)
{
	m_frameIndex = currentFrameIndex;

	// flag all sets allocated in frame frameIndexToRelease as free
	for (auto &p : m_layoutToSets)
	{
		auto &pooledSets = p.second;
		assert(pooledSets);

		pooledSets->update(currentFrameIndex, frameIndexToRelease);
	}
}

VEngine::VKDescriptorSetCache::DescriptorSetPools::DescriptorSetPools(VkDescriptorSetLayout layout, uint32_t *typeCounts)
	:m_layout(layout)
{
	memcpy(m_typeCounts, typeCounts, sizeof(m_typeCounts));
}

VEngine::VKDescriptorSetCache::DescriptorSetPools::~DescriptorSetPools()
{
	for (auto &pool : m_pools)
	{
		assert(pool.m_pool != VK_NULL_HANDLE);
		vkDestroyDescriptorPool(g_context.m_device, pool.m_pool, nullptr);
	}
}

VkDescriptorSet VEngine::VKDescriptorSetCache::DescriptorSetPools::getDescriptorSet(size_t currentFrameIndex)
{
	// look for existing free set
	for (auto &pool : m_pools)
	{
		for (size_t i = 0; i < SETS_PER_POOL; ++i)
		{
			if (pool.m_freeSetsMask[i])
			{
				// set index of frame in which the set was used
				pool.m_frameIndices[i] = currentFrameIndex;
				pool.m_freeSetsMask[i] = 0;

				return pool.m_sets[i];
			}
		}
	}

	// we're still here, so none of the existing pools have a free set and we need to create a new pool
	m_pools.push_back({});
	Pool &pool = m_pools.back();
	{
		// create pool
		{
			VkDescriptorPoolSize poolSizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

			uint32_t poolSizeCount = 0;

			for (size_t i = 0; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i)
			{
				if (m_typeCounts[i])
				{
					poolSizes[poolSizeCount].type = static_cast<VkDescriptorType>(i);
					poolSizes[poolSizeCount].descriptorCount = m_typeCounts[i] * SETS_PER_POOL;
					++poolSizeCount;
				}
			}

			assert(poolSizeCount);

			VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
			poolCreateInfo.maxSets = SETS_PER_POOL;
			poolCreateInfo.poolSizeCount = poolSizeCount;
			poolCreateInfo.pPoolSizes = poolSizes;

			if (vkCreateDescriptorPool(g_context.m_device, &poolCreateInfo, nullptr, &pool.m_pool) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create DescriptorPool!", EXIT_FAILURE);
			}
		}

		// allocate sets from pool
		{
			VkDescriptorSetLayout layouts[SETS_PER_POOL];

			for (size_t i = 0; i < SETS_PER_POOL; ++i)
			{
				layouts[i] = m_layout;
			}

			VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			setAllocInfo.descriptorPool = pool.m_pool;
			setAllocInfo.descriptorSetCount = SETS_PER_POOL;
			setAllocInfo.pSetLayouts = layouts;

			if (vkAllocateDescriptorSets(g_context.m_device, &setAllocInfo, pool.m_sets) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to allocate DescriptorSets!", EXIT_FAILURE);
			}
		}

		// set free bits
		pool.m_freeSetsMask = 0;
		pool.m_freeSetsMask.flip();
	}

	// we now have a new block with free sets; use the first one
	{
		// set index of frame in which the set was used
		pool.m_frameIndices[0] = currentFrameIndex;
		pool.m_freeSetsMask[0] = 0;
	}

	return pool.m_sets[0];
}

void VEngine::VKDescriptorSetCache::DescriptorSetPools::update(size_t currentFrameIndex, size_t frameIndexToRelease)
{
	for (auto &pool : m_pools)
	{
		// check if sets in use can be flagged as free again
		for (size_t i = 0; i < SETS_PER_POOL; ++i)
		{
			if (pool.m_frameIndices[i] == frameIndexToRelease && (pool.m_freeSetsMask[i]) == 0)
			{
				// flag as free
				pool.m_freeSetsMask[i] = 1;
			}
		}
	}
}


VEngine::VKDescriptorSetWriter::VKDescriptorSetWriter(VkDevice device, VkDescriptorSet set)
	:m_device(device),
	m_set(set)
{
}

void VEngine::VKDescriptorSetWriter::writeImageInfo(VkDescriptorType type, const VkDescriptorImageInfo *imageInfo, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	size_t currentOffset = m_imageInfo.size();

	for (uint32_t i = 0; i < count; ++i)
	{
		m_imageInfo.push_back({ imageInfo[i] });
	}

	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_set };
	write.dstBinding = binding;
	write.dstArrayElement = arrayElement;
	write.descriptorCount = count;
	write.descriptorType = type;
	write.pImageInfo = reinterpret_cast<const VkDescriptorImageInfo *>(currentOffset);

	m_writes.push_back(write);
}

void VEngine::VKDescriptorSetWriter::writeImageInfo(VkDescriptorType type, const VkDescriptorImageInfo &imageInfo, uint32_t binding, uint32_t arrayElement)
{
	size_t currentOffset = m_imageInfo.size();
	m_imageInfo.push_back(imageInfo);

	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_set };
	write.dstBinding = binding;
	write.dstArrayElement = arrayElement;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = reinterpret_cast<const VkDescriptorImageInfo *>(currentOffset);

	m_writes.push_back(write);
}

void VEngine::VKDescriptorSetWriter::writeBufferInfo(VkDescriptorType type, const VkDescriptorBufferInfo *bufferInfo, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	size_t currentOffset = m_bufferInfo.size();

	for (uint32_t i = 0; i < count; ++i)
	{
		m_bufferInfo.push_back({ bufferInfo[i] });
	}

	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_set };
	write.dstBinding = binding;
	write.dstArrayElement = arrayElement;
	write.descriptorCount = count;
	write.descriptorType = type;
	write.pBufferInfo = reinterpret_cast<const VkDescriptorBufferInfo *>(currentOffset);

	m_writes.push_back(write);
}

void VEngine::VKDescriptorSetWriter::writeBufferInfo(VkDescriptorType type, const VkDescriptorBufferInfo &bufferInfo, uint32_t binding, uint32_t arrayElement)
{
	size_t currentOffset = m_bufferInfo.size();
	m_bufferInfo.push_back(bufferInfo);

	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_set };
	write.dstBinding = binding;
	write.dstArrayElement = arrayElement;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = reinterpret_cast<const VkDescriptorBufferInfo *>(currentOffset);

	m_writes.push_back(write);
}

void VEngine::VKDescriptorSetWriter::writeBufferView(VkDescriptorType type, const VkBufferView *bufferView, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	size_t currentOffset = m_bufferViews.size();

	for (uint32_t i = 0; i < count; ++i)
	{
		m_bufferViews.push_back(bufferView[i]);
	}

	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_set };
	write.dstBinding = binding;
	write.dstArrayElement = arrayElement;
	write.descriptorCount = count;
	write.descriptorType = type;
	write.pTexelBufferView = reinterpret_cast<const VkBufferView *>(currentOffset);

	m_writes.push_back(write);
}

void VEngine::VKDescriptorSetWriter::writeBufferView(VkDescriptorType type, const VkBufferView &bufferView, uint32_t binding, uint32_t arrayElement)
{
	size_t currentOffset = m_bufferViews.size();
	m_bufferViews.push_back(bufferView);

	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_set };
	write.dstBinding = binding;
	write.dstArrayElement = arrayElement;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pTexelBufferView = reinterpret_cast<const VkBufferView *>(currentOffset);

	m_writes.push_back(write);
}

void VEngine::VKDescriptorSetWriter::commit()
{
	for (auto &write : m_writes)
	{
		write.pImageInfo = m_imageInfo.data() + (size_t)write.pImageInfo;
		write.pBufferInfo = m_bufferInfo.data() + (size_t)write.pBufferInfo;
		write.pTexelBufferView = m_bufferViews.data() + (size_t)write.pTexelBufferView;
	}

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);
}

void VEngine::VKDescriptorSetWriter::clear()
{
	m_writes.clear();
	m_imageInfo.clear();
	m_bufferInfo.clear();
	m_bufferViews.clear();
}