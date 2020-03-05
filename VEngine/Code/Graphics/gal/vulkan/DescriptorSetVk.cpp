#include "DescriptorSetVk.h"
#include <algorithm>
#include <new>
#include "UtilityVk.h"
#include "ResourceVk.h"

VEngine::gal::DescriptorSetLayoutVk::DescriptorSetLayoutVk(VkDevice device, uint32_t bindingCount, const VkDescriptorSetLayoutBinding *bindings, uint32_t *typeCounts)
	:m_device(device),
	m_descriptorSetLayout(VK_NULL_HANDLE),
	m_typeCounts()
{
	VkDescriptorSetLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	createInfo.bindingCount = bindingCount;
	createInfo.pBindings = bindings;

	UtilityVk::checkResult(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &m_descriptorSetLayout), "Failed to create DescriptorSetLayout!");

	memcpy(m_typeCounts.m_typeCounts, typeCounts, sizeof(m_typeCounts.m_typeCounts));
}

VEngine::gal::DescriptorSetLayoutVk::~DescriptorSetLayoutVk()
{
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
}

void *VEngine::gal::DescriptorSetLayoutVk::getNativeHandle() const
{
	return m_descriptorSetLayout;
}

const VEngine::gal::DescriptorSetLayoutTypeCounts &VEngine::gal::DescriptorSetLayoutVk::getTypeCounts() const
{
	return m_typeCounts;
}

VEngine::gal::DescriptorSetVk::DescriptorSetVk(VkDevice device, VkDescriptorSet descriptorSet)
	:m_device(device),
	m_descriptorSet(descriptorSet)
{
}

void *VEngine::gal::DescriptorSetVk::getNativeHandle() const
{
	return m_descriptorSet;
}

void VEngine::gal::DescriptorSetVk::update(uint32_t count, const DescriptorSetUpdate *updates)
{
	constexpr uint32_t batchSize = 16;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, count - i * batchSize);

		union InfoData
		{
			VkDescriptorImageInfo m_imageInfo;
			VkDescriptorBufferInfo m_bufferInfo;
			VkBufferView m_texelBufferView;
		};

		InfoData infoDataVk[batchSize];
		VkWriteDescriptorSet writesVk[batchSize];

		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &update = updates[i * batchSize + j];

			auto &write = writesVk[j];
			write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write.dstSet = m_descriptorSet;
			write.dstBinding = update.m_dstBinding;
			write.dstArrayElement = update.m_dstArrayElement;
			write.descriptorCount = update.m_descriptorCount;

			auto &info = infoDataVk[j];

			switch (update.m_descriptorType)
			{
			case DescriptorType::SAMPLER:
			{
				info.m_imageInfo = { (VkSampler)update.m_sampler->getNativeHandle(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED };
				write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				write.pImageInfo = &info.m_imageInfo;
				break;
			}
			case DescriptorType::SAMPLED_IMAGE:
			{
				info.m_imageInfo = { VK_NULL_HANDLE, (VkImageView)update.m_imageView->getNativeHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
				write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				write.pImageInfo = &info.m_imageInfo;
				break;
			}
			case DescriptorType::STORAGE_IMAGE:
			{
				info.m_imageInfo = { VK_NULL_HANDLE, (VkImageView)update.m_imageView->getNativeHandle(), VK_IMAGE_LAYOUT_GENERAL };
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				write.pImageInfo = &info.m_imageInfo;
				break;
			}
			case DescriptorType::UNIFORM_TEXEL_BUFFER:
			{
				info.m_texelBufferView = (VkBufferView)update.m_bufferView->getNativeHandle();
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				write.pTexelBufferView = &info.m_texelBufferView;
				break;
			}
			case DescriptorType::STORAGE_TEXEL_BUFFER:
			{
				info.m_texelBufferView = (VkBufferView)update.m_bufferView->getNativeHandle();
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
				write.pTexelBufferView = &info.m_texelBufferView;
				break;
			}
			case DescriptorType::UNIFORM_BUFFER:
			{
				const auto *bufferVk = dynamic_cast<const BufferVk *>(update.m_buffer);
				assert(bufferVk);

				info.m_bufferInfo = { (VkBuffer)update.m_buffer->getNativeHandle(), update.m_bufferOffset, update.m_bufferRange };
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				write.pBufferInfo = &info.m_bufferInfo;
				break;
			}
			case DescriptorType::STORAGE_BUFFER:
			{
				const auto *bufferVk = dynamic_cast<const BufferVk *>(update.m_buffer);
				assert(bufferVk);

				info.m_bufferInfo = { (VkBuffer)update.m_buffer->getNativeHandle(), update.m_bufferOffset, update.m_bufferRange };
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				write.pBufferInfo = &info.m_bufferInfo;
				break;
			}
			default:
				assert(false);
				break;
			}
		}

		vkUpdateDescriptorSets(m_device, countVk, writesVk, 0, nullptr);
	}
}

VEngine::gal::DescriptorPoolVk::DescriptorPoolVk(VkDevice device, uint32_t maxSets, const uint32_t typeCounts[VK_DESCRIPTOR_TYPE_RANGE_SIZE])
	:m_device(device),
	m_descriptorPool(VK_NULL_HANDLE),
	m_descriptorSetMemoryPool(maxSets)
{
	VkDescriptorPoolSize poolSizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

	uint32_t poolSizeCount = 0;

	for (size_t i = 0; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i)
	{
		if (typeCounts[i])
		{
			poolSizes[poolSizeCount].type = static_cast<VkDescriptorType>(i);
			poolSizes[poolSizeCount].descriptorCount = typeCounts[i] * maxSets;
			++poolSizeCount;
		}
	}

	assert(poolSizeCount);

	VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolCreateInfo.maxSets = maxSets;
	poolCreateInfo.poolSizeCount = poolSizeCount;
	poolCreateInfo.pPoolSizes = poolSizes;

	UtilityVk::checkResult(vkCreateDescriptorPool(m_device, &poolCreateInfo, nullptr, &m_descriptorPool), "Failed to create DescriptorPool!");
}

VEngine::gal::DescriptorPoolVk::~DescriptorPoolVk()
{
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	// normally we would have to manually call the destructor on all currently allocated sets,
	// which would require keeping a list of all sets. since DescriptorSetVk is a POD, we can
	// simply let the DynamicObjectPool destructor scrap the backing memory without first doing this.
}

void *VEngine::gal::DescriptorPoolVk::getNativeHandle() const
{
	return m_descriptorPool;
}

void VEngine::gal::DescriptorPoolVk::allocateDescriptorSets(uint32_t count, const DescriptorSetLayout **layouts, DescriptorSet **sets)
{
	constexpr uint32_t batchSize = 8;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkDescriptorSetLayout layoutsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			layoutsVk[j] = (VkDescriptorSetLayout)layouts[i * batchSize + j]->getNativeHandle();
		}

		VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = countVk;
		allocInfo.pSetLayouts = layoutsVk;

		VkDescriptorSet setsVk[batchSize];

		UtilityVk::checkResult(vkAllocateDescriptorSets(m_device, &allocInfo, setsVk), "Failed to allocate Descriptor Sets from Descriptor Pool!");

		for (uint32_t j = 0; j < countVk; ++j)
		{
			auto *memory = m_descriptorSetMemoryPool.alloc();
			assert(memory);
			sets[i * batchSize + j] = new(memory) DescriptorSetVk(m_device, setsVk[j]);
		}
	}
}

void VEngine::gal::DescriptorPoolVk::freeDescriptorSets(uint32_t count, DescriptorSet **sets)
{
	constexpr uint32_t batchSize = 8;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkDescriptorSet setsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			setsVk[j] = (VkDescriptorSet)sets[i * batchSize + j]->getNativeHandle();
		}
		UtilityVk::checkResult(vkFreeDescriptorSets(m_device, m_descriptorPool, countVk, setsVk), "Failed to free Descriptor Sets!");
		for (uint32_t j = 0; j < countVk; ++j)
		{
			auto *descriptorSetVk = dynamic_cast<DescriptorSetVk *>(sets[i * batchSize + j]);
			assert(descriptorSetVk);

			// call destructor and free backing memory
			descriptorSetVk->~DescriptorSetVk();
			m_descriptorSetMemoryPool.free(reinterpret_cast<ByteArray<sizeof(DescriptorSetVk)> *>(descriptorSetVk));
		}
	}
}
