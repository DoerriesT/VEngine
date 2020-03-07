#pragma once
#include "gal/GraphicsAbstractionLayer.h"
#include <cstdint>
#include <bitset>
#include <vector>
#include <map>
#include <memory>

namespace VEngine
{
	class DescriptorSetCache2
	{
	public:
		DescriptorSetCache2(gal::GraphicsDevice *graphicsDevice);
		DescriptorSetCache2(DescriptorSetCache2 &) = delete;
		DescriptorSetCache2(DescriptorSetCache2 &&) = delete;
		DescriptorSetCache2 &operator= (const DescriptorSetCache2 &) = delete;
		DescriptorSetCache2 &operator= (const DescriptorSetCache2 &&) = delete;

		// typeCounts is an array with gal::DescriptorType::RANGE_SIZE elements holding the count of descriptor types in the layout
		gal::DescriptorSet *getDescriptorSet(gal::DescriptorSetLayout *layout, uint32_t * typeCounts);
		// index of current frame and index of frame which finished rendering and whose resources may be freed
		void update(uint64_t currentFrameIndex, uint64_t frameIndexToRelease);

	private:

		// holds a vector of pools for a specific layout that dynamically grows to satisfy requests
		struct DescriptorSetPools2
		{
			enum { SETS_PER_POOL = 16 };

			// a single pool with a fixed number of preallocated sets
			struct Pool
			{
				gal::DescriptorSet *m_sets[SETS_PER_POOL];
				uint64_t m_frameIndices[SETS_PER_POOL];
				std::bitset<SETS_PER_POOL> m_freeSetsMask;
				gal::DescriptorPool *m_pool = nullptr;
			};

			gal::GraphicsDevice *m_graphicsDevice;
			uint32_t m_typeCounts[(size_t)gal::DescriptorType::RANGE_SIZE];
			gal::DescriptorSetLayout *m_layout;
			std::vector<Pool> m_pools;

			explicit DescriptorSetPools2(gal::GraphicsDevice *graphicsDevice, gal::DescriptorSetLayout *layout, uint32_t *typeCounts);
			DescriptorSetPools2(DescriptorSetPools2 &) = delete;
			DescriptorSetPools2(DescriptorSetPools2 &&) = delete;
			DescriptorSetPools2 &operator= (const DescriptorSetPools2 &) = delete;
			DescriptorSetPools2 &operator= (const DescriptorSetPools2 &&) = delete;
			~DescriptorSetPools2();

			gal::DescriptorSet *getDescriptorSet(uint64_t currentFrameIndex);
			void update(uint64_t currentFrameIndex, uint64_t frameIndexToRelease);
		};

		gal::GraphicsDevice *m_graphicsDevice;
		uint64_t m_frameIndex;
		std::map<gal::DescriptorSetLayout *, std::unique_ptr<DescriptorSetPools2>> m_layoutToSets;
	};
}