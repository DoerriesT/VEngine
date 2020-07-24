#include "DescriptorSetVk.h"
#include <algorithm>
#include <new>
#include "UtilityVk.h"
#include "ResourceVk.h"
#include <cassert>
#include <vector>
#include "Utility/Utility.h"

VEngine::gal::DescriptorSetLayoutVk::DescriptorSetLayoutVk(VkDevice device, uint32_t bindingCount, const VkDescriptorSetLayoutBinding *bindings)
	:m_device(device),
	m_descriptorSetLayout(VK_NULL_HANDLE),
	m_typeCounts()
{
	for (uint32_t i = 0; i < bindingCount; ++i)
	{
		switch (bindings[i].descriptorType)
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			m_typeCounts[static_cast<size_t>(DescriptorType::SAMPLER)] += bindings[i].descriptorCount;
			break;
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			m_typeCounts[static_cast<size_t>(DescriptorType::SAMPLED_IMAGE)] += bindings[i].descriptorCount;
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			m_typeCounts[static_cast<size_t>(DescriptorType::STORAGE_IMAGE)] += bindings[i].descriptorCount;
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			m_typeCounts[static_cast<size_t>(DescriptorType::UNIFORM_TEXEL_BUFFER)] += bindings[i].descriptorCount;
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			m_typeCounts[static_cast<size_t>(DescriptorType::STORAGE_TEXEL_BUFFER)] += bindings[i].descriptorCount;
			break;
		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			m_typeCounts[static_cast<size_t>(DescriptorType::UNIFORM_BUFFER)] += bindings[i].descriptorCount;
			break;
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			m_typeCounts[static_cast<size_t>(DescriptorType::STORAGE_BUFFER)] += bindings[i].descriptorCount;
			break;
		default:
			// unsupported descriptor type
			assert(false);
			break;
		}
	}

	VkDescriptorSetLayoutCreateInfo createInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	createInfo.bindingCount = bindingCount;
	createInfo.pBindings = bindings;

	UtilityVk::checkResult(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &m_descriptorSetLayout), "Failed to create DescriptorSetLayout!");
}

VEngine::gal::DescriptorSetLayoutVk::~DescriptorSetLayoutVk()
{
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
}

void *VEngine::gal::DescriptorSetLayoutVk::getNativeHandle() const
{
	return m_descriptorSetLayout;
}

const uint32_t *VEngine::gal::DescriptorSetLayoutVk::getTypeCounts() const
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

		std::vector<VkDescriptorImageInfo> imageInfos;
		std::vector<VkDescriptorBufferInfo> bufferInfos;
		std::vector<VkBufferView> texelBufferViews;

		size_t imageInfoReserveCount = 0;
		size_t bufferInfoReserveCount = 0;
		size_t texelBufferViewsReserveCount = 0;
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &update = updates[i * batchSize + j];
			switch (update.m_descriptorType)
			{
			case DescriptorType::SAMPLER:
			case DescriptorType::SAMPLED_IMAGE:
			case DescriptorType::STORAGE_IMAGE:
				imageInfoReserveCount += update.m_descriptorCount;
				break;
			case DescriptorType::UNIFORM_TEXEL_BUFFER:
			case DescriptorType::STORAGE_TEXEL_BUFFER:
				texelBufferViewsReserveCount += update.m_descriptorCount;
				break;
			case DescriptorType::UNIFORM_BUFFER:
			case DescriptorType::STORAGE_BUFFER:
				bufferInfoReserveCount += update.m_descriptorCount;
				break;
			default:
				assert(false);
				break;
			}
		}

		imageInfos.reserve(imageInfoReserveCount);
		bufferInfos.reserve(bufferInfoReserveCount);
		texelBufferViews.reserve(texelBufferViewsReserveCount);

		//union InfoData
		//{
		//	VkDescriptorImageInfo m_imageInfo;
		//	VkDescriptorBufferInfo m_bufferInfo;
		//	VkBufferView m_texelBufferView;
		//};
		//
		//InfoData infoDataVk[batchSize];
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

			//auto &info = infoDataVk[j];

			switch (update.m_descriptorType)
			{
			case DescriptorType::SAMPLER:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					imageInfos.push_back({ (VkSampler)update.m_samplers[k]->getNativeHandle(), VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED });
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
				write.pImageInfo = imageInfos.data() + imageInfos.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::SAMPLED_IMAGE:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					imageInfos.push_back({ VK_NULL_HANDLE, (VkImageView)update.m_imageViews[k]->getNativeHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				write.pImageInfo = imageInfos.data() + imageInfos.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::STORAGE_IMAGE:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					imageInfos.push_back({ VK_NULL_HANDLE, (VkImageView)update.m_imageViews[k]->getNativeHandle(), VK_IMAGE_LAYOUT_GENERAL });
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				write.pImageInfo = imageInfos.data() + imageInfos.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::UNIFORM_TEXEL_BUFFER:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					texelBufferViews.push_back((VkBufferView)update.m_bufferViews[k]->getNativeHandle());
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
				write.pTexelBufferView = texelBufferViews.data() + texelBufferViews.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::STORAGE_TEXEL_BUFFER:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					texelBufferViews.push_back((VkBufferView)update.m_bufferViews[k]->getNativeHandle());
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
				write.pTexelBufferView = texelBufferViews.data() + texelBufferViews.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::UNIFORM_BUFFER:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					const auto &info = update.m_bufferInfo[k];
					const auto *bufferVk = dynamic_cast<const BufferVk *>(info.m_buffer);
					assert(bufferVk);
					bufferInfos.push_back({ (VkBuffer)bufferVk->getNativeHandle(), info.m_offset, info.m_range });
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				write.pBufferInfo = bufferInfos.data() + bufferInfos.size() - update.m_descriptorCount;
				break;
			}
			case DescriptorType::STORAGE_BUFFER:
			{
				for (size_t k = 0; k < update.m_descriptorCount; ++k)
				{
					const auto &info = update.m_bufferInfo[k];
					const auto *bufferVk = dynamic_cast<const BufferVk *>(info.m_buffer);
					assert(bufferVk);
					bufferInfos.push_back({ (VkBuffer)bufferVk->getNativeHandle(), info.m_offset, info.m_range });
				}
				write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				write.pBufferInfo = bufferInfos.data() + bufferInfos.size() - update.m_descriptorCount;
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

VEngine::gal::DescriptorSetPoolVk::DescriptorSetPoolVk(VkDevice device, uint32_t maxSets, const DescriptorSetLayoutVk *layout)
	:m_device(device),
	m_descriptorPool(VK_NULL_HANDLE),
	m_layout(layout),
	m_poolSize(maxSets),
	m_currentOffset(),
	m_descriptorSetMemory()
{
	VkDescriptorPoolSize poolSizes[(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1)];

	uint32_t poolSizeCount = 0;

	const uint32_t *typeCounts = layout->getTypeCounts();

	for (size_t i = 0; i < (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1); ++i)
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

	// allocate memory to placement new DescriptorSetVk
	m_descriptorSetMemory = new char[sizeof(DescriptorSetVk) * m_poolSize];
}

VEngine::gal::DescriptorSetPoolVk::~DescriptorSetPoolVk()
{
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	// normally we would have to manually call the destructor on all currently allocated sets,
	// which would require keeping a list of all sets. since DescriptorSetVk is a POD, we can
	// simply scrap the backing memory without first doing this.

	delete[] m_descriptorSetMemory;
	m_descriptorSetMemory = nullptr;
}

void *VEngine::gal::DescriptorSetPoolVk::getNativeHandle() const
{
	return m_descriptorPool;
}

void VEngine::gal::DescriptorSetPoolVk::allocateDescriptorSets(uint32_t count, DescriptorSet **sets)
{
	if (m_currentOffset + count > m_poolSize)
	{
		Utility::fatalExit("Tried to allocate more descriptor sets from descriptor set pool than available!", EXIT_FAILURE);
	}

	VkDescriptorSetLayout layoutVk = (VkDescriptorSetLayout)m_layout->getNativeHandle();

	constexpr uint32_t batchSize = 8;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkDescriptorSetLayout layoutsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			layoutsVk[j] = layoutVk;
		}

		VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = countVk;
		allocInfo.pSetLayouts = layoutsVk;

		VkDescriptorSet setsVk[batchSize];

		UtilityVk::checkResult(vkAllocateDescriptorSets(m_device, &allocInfo, setsVk), "Failed to allocate Descriptor Sets from Descriptor Pool!");

		for (uint32_t j = 0; j < countVk; ++j)
		{
			sets[i * batchSize + j] = new(m_descriptorSetMemory + sizeof(DescriptorSetVk) * m_currentOffset) DescriptorSetVk(m_device, setsVk[j]);
			++m_currentOffset;
		}
	}
}

void VEngine::gal::DescriptorSetPoolVk::reset()
{
	UtilityVk::checkResult(vkResetDescriptorPool(m_device, m_descriptorPool, 0), "Failed to reset descriptor pool!");

	// DescriptorSetVk is a POD class, so we can simply reset the allocator offset of the backing memory
	// and dont need to call the destructors
	m_currentOffset = 0;
}