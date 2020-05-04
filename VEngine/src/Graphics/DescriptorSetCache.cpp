#include "DescriptorSetCache.h"
#include <cassert>

using namespace VEngine::gal;

VEngine::DescriptorSetCache::DescriptorSetCache(GraphicsDevice *graphicsDevice)
	:m_graphicsDevice(graphicsDevice),
	m_frameIndex(),
	m_layoutToSets()
{
}

VEngine::gal::DescriptorSet *VEngine::DescriptorSetCache::getDescriptorSet(const DescriptorSetLayout *layout)
{
	auto &pooledSetsPtr = m_layoutToSets[layout];

	// layout doesnt exist yet -> create it
	if (!pooledSetsPtr)
	{
		pooledSetsPtr = std::make_unique<DescriptorSetPools>(m_graphicsDevice, layout);
	}

	return pooledSetsPtr->getDescriptorSet(m_frameIndex);
}

void VEngine::DescriptorSetCache::update(uint64_t currentFrameIndex, uint64_t frameIndexToRelease)
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

VEngine::DescriptorSetCache::DescriptorSetPools::DescriptorSetPools(GraphicsDevice *graphicsDevice, const DescriptorSetLayout *layout)
	:m_graphicsDevice(graphicsDevice),
	m_layout(layout),
	m_pools()
{
}

VEngine::DescriptorSetCache::DescriptorSetPools::~DescriptorSetPools()
{
	for (auto &pool : m_pools)
	{
		assert(pool.m_pool);
		m_graphicsDevice->destroyDescriptorSetPool(pool.m_pool);
	}
}

VEngine::gal::DescriptorSet *VEngine::DescriptorSetCache::DescriptorSetPools::getDescriptorSet(uint64_t currentFrameIndex)
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
		m_graphicsDevice->createDescriptorSetPool(SETS_PER_POOL, m_layout, &pool.m_pool);

		// allocate sets from pool
		pool.m_pool->allocateDescriptorSets(SETS_PER_POOL, pool.m_sets);

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

void VEngine::DescriptorSetCache::DescriptorSetPools::update(uint64_t currentFrameIndex, uint64_t frameIndexToRelease)
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
