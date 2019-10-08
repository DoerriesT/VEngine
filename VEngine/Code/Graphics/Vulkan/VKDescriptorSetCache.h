#pragma once
#include "Graphics/Vulkan/volk.h"
#include <map>
#include <bitset>
#include <vector>
#include <memory>

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

		// typeCounts is an array with VK_DESCRIPTOR_TYPE_RANGE_SIZE elements holding the count of descriptor types in the layout
		VkDescriptorSet getDescriptorSet(VkDescriptorSetLayout layout, uint32_t *typeCounts);
		// index of current frame and index of frame which finished rendering and whose resources may be freed
		void update(size_t currentFrameIndex, size_t frameIndexToRelease);

	private:
		
		// holds a vector of pools for a specific layout that dynamically grows to satisfy requests
		struct DescriptorSetPools
		{
			enum { SETS_PER_POOL = 16 };

			// a single pool with a fixed number of preallocated sets
			struct Pool
			{
				VkDescriptorSet m_sets[SETS_PER_POOL];
				size_t m_frameIndices[SETS_PER_POOL];
				std::bitset<SETS_PER_POOL> m_freeSetsMask;
				VkDescriptorPool m_pool = VK_NULL_HANDLE;
			};

			uint32_t m_typeCounts[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
			VkDescriptorSetLayout m_layout;
			std::vector<Pool> m_pools;

			explicit DescriptorSetPools(VkDescriptorSetLayout layout, uint32_t *typeCounts);
			DescriptorSetPools(DescriptorSetPools &) = delete;
			DescriptorSetPools(DescriptorSetPools &&) = delete;
			DescriptorSetPools &operator= (const DescriptorSetPools &) = delete;
			DescriptorSetPools &operator= (const DescriptorSetPools &&) = delete;
			~DescriptorSetPools();

			VkDescriptorSet getDescriptorSet(size_t currentFrameIndex);
			void update(size_t currentFrameIndex, size_t frameIndexToRelease);
		};

		size_t m_frameIndex;
		std::map<VkDescriptorSetLayout, std::unique_ptr<DescriptorSetPools>> m_layoutToSets;
	};

	class VKDescriptorSetWriter
	{
	public:
		explicit VKDescriptorSetWriter(VkDevice device, VkDescriptorSet set);
		void writeImageInfo(VkDescriptorType type, const VkDescriptorImageInfo *imageInfo, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		void writeImageInfo(VkDescriptorType type, const VkDescriptorImageInfo &imageInfo, uint32_t binding, uint32_t arrayElement = 0);
		void writeBufferInfo(VkDescriptorType type, const VkDescriptorBufferInfo *bufferInfo, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		void writeBufferInfo(VkDescriptorType type, const VkDescriptorBufferInfo &bufferInfo, uint32_t binding, uint32_t arrayElement = 0);
		void writeBufferView(VkDescriptorType type, const VkBufferView *bufferView, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
		void writeBufferView(VkDescriptorType type, const VkBufferView &bufferView, uint32_t binding, uint32_t arrayElement = 0);
		void commit();
		void clear();

	private:
		VkDevice m_device;
		VkDescriptorSet m_set;
		std::vector<VkWriteDescriptorSet> m_writes;
		std::vector<VkDescriptorImageInfo> m_imageInfo;
		std::vector<VkDescriptorBufferInfo> m_bufferInfo;
		std::vector<VkBufferView> m_bufferViews;
	};
}