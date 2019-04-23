#include "VKDescriptorSetCache.h"
#include "VKContext.h"
#include "Utility/Utility.h"

VEngine::VKDescriptorSetCache::~VKDescriptorSetCache()
{
	// free all pools
	for (auto &p : m_layoutToSets)
	{
		auto &pooledSets = p.second;

		assert(pooledSets.m_pool != VK_NULL_HANDLE);

		vkDestroyDescriptorPool(g_context.m_device, pooledSets.m_pool, nullptr);
	}
}

VkDescriptorSet VEngine::VKDescriptorSetCache::getDescriptorSet(VkDescriptorSetLayout layout, uint32_t *typeCounts)
{
	auto &pooledSets = m_layoutToSets[layout];

	// layout doesnt exist yet -> create it
	if (pooledSets.m_pool == VK_NULL_HANDLE)
	{
		// create pool
		{
			VkDescriptorPoolSize poolSizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

			uint32_t poolSizeCount = 0;

			for (size_t i = 0; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i)
			{
				if (typeCounts[i])
				{
					poolSizes[poolSizeCount].type = static_cast<VkDescriptorType>(i);
					poolSizes[poolSizeCount].descriptorCount = typeCounts[i] * SETS_PER_LAYOUT;
					++poolSizeCount;
				}
			}
			
			assert(poolSizeCount);

			VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
			poolCreateInfo.maxSets = SETS_PER_LAYOUT;
			poolCreateInfo.poolSizeCount = poolSizeCount;
			poolCreateInfo.pPoolSizes = poolSizes;

			if (vkCreateDescriptorPool(g_context.m_device, &poolCreateInfo, nullptr, &pooledSets.m_pool) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create DescriptorPool!", EXIT_FAILURE);
			}
		}
		
		// allocate sets from pool
		{
			VkDescriptorSetLayout layouts[SETS_PER_LAYOUT];

			for (size_t i = 0; i < SETS_PER_LAYOUT; ++i)
			{
				layouts[i] = layout;
			}

			VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			setAllocInfo.descriptorPool = pooledSets.m_pool;
			setAllocInfo.descriptorSetCount = SETS_PER_LAYOUT;
			setAllocInfo.pSetLayouts = layouts;

			if (vkAllocateDescriptorSets(g_context.m_device, &setAllocInfo, pooledSets.m_sets) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to allocate DescriptorSets!", EXIT_FAILURE);
			}
		}

		// set free bits
		pooledSets.m_freeSetsMask = ~uint16_t(0);
	}

	// find free set
	{
		if (!pooledSets.m_freeSetsMask)
		{
			Utility::fatalExit("No more free DescriptorSets available for requested layout!", EXIT_FAILURE);
		}

		for (size_t i = 0; i < 16; ++i)
		{
			if (pooledSets.m_freeSetsMask & (1u << i))
			{
				// set index of frame in which the set was used
				pooledSets.m_frameIndices[i] = m_frameIndex;
				pooledSets.m_freeSetsMask &= ~(1u << i);

				return pooledSets.m_sets[i];
			}
		}
	}

	assert(false);
	return VK_NULL_HANDLE;
}

void VEngine::VKDescriptorSetCache::update(size_t currentFrameIndex, size_t frameIndexToRelease)
{
	m_frameIndex = currentFrameIndex;

	// flag all sets allocated in frame frameIndexToRelease as free
	for (auto &p : m_layoutToSets)
	{
		auto &pooledSets = p.second;

		// skip if all sets are free already
		if (pooledSets.m_freeSetsMask == ~uint16_t(0))
		{
			continue;
		}

		// check if sets in use can be flagged as free again
		for (size_t i = 0; i < 16; ++i)
		{
			if (pooledSets.m_frameIndices[i] == frameIndexToRelease && (pooledSets.m_freeSetsMask & (1u << i)) == 0)
			{
				// flag as free
				pooledSets.m_freeSetsMask |= 1u << i;
			}
		}
	}
}
