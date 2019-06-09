#pragma once
#include "Graphics/Vulkan/volk.h"
#include <map>
#include <bitset>

namespace VEngine
{
	class VKDescriptorSetCache
	{
	public:
		VKDescriptorSetCache() = default;
		VKDescriptorSetCache(VKDescriptorSetCache &) = delete;
		VKDescriptorSetCache(VKDescriptorSetCache &&) = delete;
		VKDescriptorSetCache &operator= (const VKDescriptorSetCache &) = delete;
		VKDescriptorSetCache &operator= (const VKDescriptorSetCache &&) = delete;
		~VKDescriptorSetCache();

		// typeCounts is an array with VK_DESCRIPTOR_TYPE_RANGE_SIZE elements holding the count of descriptor types in the layout
		VkDescriptorSet getDescriptorSet(VkDescriptorSetLayout layout, uint32_t *typeCounts);
		// index of current frame and index of frame which finished rendering and whose resources may be freed
		void update(size_t currentFrameIndex, size_t frameIndexToRelease);

	private:
		enum 
		{
			SETS_PER_LAYOUT = 64
		};

		struct PooledDescriptorSets
		{
			VkDescriptorSet m_sets[SETS_PER_LAYOUT];
			size_t m_frameIndices[SETS_PER_LAYOUT];
			std::bitset<SETS_PER_LAYOUT> m_freeSetsMask;
			VkDescriptorPool m_pool = VK_NULL_HANDLE;
		};

		size_t m_frameIndex;
		std::map<VkDescriptorSetLayout, PooledDescriptorSets> m_layoutToSets;
	};
}