#pragma once
#include "Graphics/gal/DescriptorSet.h"
#include "Graphics/Vulkan/volk.h"
#include "Utility/ObjectPool.h"

namespace VEngine
{
	namespace gal
	{
		class DescriptorSetLayoutVk : public DescriptorSetLayout
		{
		public:
			explicit DescriptorSetLayoutVk(VkDevice device, uint32_t bindingCount, const VkDescriptorSetLayoutBinding *bindings, uint32_t *typeCounts);
			DescriptorSetLayoutVk(DescriptorSetLayoutVk &) = delete;
			DescriptorSetLayoutVk(DescriptorSetLayoutVk &&) = delete;
			DescriptorSetLayoutVk &operator= (const DescriptorSetLayoutVk &) = delete;
			DescriptorSetLayoutVk &operator= (const DescriptorSetLayoutVk &&) = delete;
			~DescriptorSetLayoutVk();
			void *getNativeHandle() const override;
			const DescriptorSetLayoutTypeCounts &getTypeCounts() const override;
		private:
			VkDevice m_device;
			VkDescriptorSetLayout m_descriptorSetLayout;
			DescriptorSetLayoutTypeCounts m_typeCounts;
		};

		class DescriptorSetVk : public DescriptorSet
		{
		public:
			explicit DescriptorSetVk(VkDevice device, VkDescriptorSet descriptorSet);
			void *getNativeHandle() const override;
			void update(uint32_t count, const DescriptorSetUpdate *updates) override;
		private:
			VkDevice m_device;
			VkDescriptorSet m_descriptorSet;
		};

		class DescriptorPoolVk : public DescriptorPool
		{
		public:
			explicit DescriptorPoolVk(VkDevice device, uint32_t maxSets, const uint32_t typeCounts[VK_DESCRIPTOR_TYPE_RANGE_SIZE]);
			DescriptorPoolVk(DescriptorPoolVk &) = delete;
			DescriptorPoolVk(DescriptorPoolVk &&) = delete;
			DescriptorPoolVk &operator= (const DescriptorPoolVk &) = delete;
			DescriptorPoolVk &operator= (const DescriptorPoolVk &&) = delete;
			~DescriptorPoolVk();
			void *getNativeHandle() const override;
			void allocateDescriptorSets(uint32_t count, const DescriptorSetLayout **layouts, DescriptorSet **sets);
			void freeDescriptorSets(uint32_t count, DescriptorSet **sets);
		private:
			VkDevice m_device;
			VkDescriptorPool m_descriptorPool;
			DynamicObjectPool<ByteArray<sizeof(DescriptorSetVk)>> m_descriptorSetMemoryPool;
		};
	}
}