#include "DescriptorSetCache.h"
#include <cassert>

using namespace VEngine::gal;

VEngine::DescriptorSetCache2::DescriptorSetCache2(GraphicsDevice *graphicsDevice)
	:m_graphicsDevice(graphicsDevice),
	m_frameIndex(),
	m_layoutToSets()
{
}

VEngine::gal::DescriptorSet *VEngine::DescriptorSetCache2::getDescriptorSet(const DescriptorSetLayout *layout)
{
	auto &pooledSetsPtr = m_layoutToSets[layout];

	// layout doesnt exist yet -> create it
	if (!pooledSetsPtr)
	{
		pooledSetsPtr = std::make_unique<DescriptorSetPools2>(m_graphicsDevice, layout);
	}

	return pooledSetsPtr->getDescriptorSet(m_frameIndex);
}

void VEngine::DescriptorSetCache2::update(uint64_t currentFrameIndex, uint64_t frameIndexToRelease)
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

VEngine::DescriptorSetCache2::DescriptorSetPools2::DescriptorSetPools2(GraphicsDevice *graphicsDevice, const DescriptorSetLayout *layout)
	:m_graphicsDevice(graphicsDevice),
	m_typeCounts(),
	m_layout(layout),
	m_pools()
{
	memcpy(m_typeCounts, layout->getTypeCounts().m_typeCounts, sizeof(m_typeCounts));
	for (auto &typeCount : m_typeCounts)
	{
		typeCount *= SETS_PER_POOL;
	}
}

VEngine::DescriptorSetCache2::DescriptorSetPools2::~DescriptorSetPools2()
{
	for (auto &pool : m_pools)
	{
		assert(pool.m_pool);
		m_graphicsDevice->destroyDescriptorPool(pool.m_pool);
	}
}

VEngine::gal::DescriptorSet *VEngine::DescriptorSetCache2::DescriptorSetPools2::getDescriptorSet(uint64_t currentFrameIndex)
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
			m_graphicsDevice->createDescriptorPool(SETS_PER_POOL, m_typeCounts, &pool.m_pool);
		}

		// allocate sets from pool
		{
			const DescriptorSetLayout *layouts[SETS_PER_POOL];
			for (size_t i = 0; i < SETS_PER_POOL; ++i)
			{
				layouts[i] = m_layout;
			}

			pool.m_pool->allocateDescriptorSets(SETS_PER_POOL, layouts, pool.m_sets);
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

void VEngine::DescriptorSetCache2::DescriptorSetPools2::update(uint64_t currentFrameIndex, uint64_t frameIndexToRelease)
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
