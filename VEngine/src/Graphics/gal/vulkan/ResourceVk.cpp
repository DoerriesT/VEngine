#include "ResourceVk.h"
#include "MemoryAllocatorVk.h"
#include "GraphicsDeviceVk.h"
#include "UtilityVk.h"
#include <algorithm>
#include "Utility/Utility.h"

VEngine::gal::SamplerVk::SamplerVk(VkDevice device, const SamplerCreateInfo &createInfo)
	: m_device(device),
	m_sampler(VK_NULL_HANDLE)
{
	VkSamplerCreateInfo createInfoVk{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	createInfoVk.magFilter = static_cast<VkFilter>(createInfo.m_magFilter);
	createInfoVk.minFilter = static_cast<VkFilter>(createInfo.m_minFilter);
	createInfoVk.mipmapMode = static_cast<VkSamplerMipmapMode>(createInfo.m_mipmapMode);
	createInfoVk.addressModeU = static_cast<VkSamplerAddressMode>(createInfo.m_addressModeU);
	createInfoVk.addressModeV = static_cast<VkSamplerAddressMode>(createInfo.m_addressModeV);
	createInfoVk.addressModeW = static_cast<VkSamplerAddressMode>(createInfo.m_addressModeW);
	createInfoVk.mipLodBias = createInfo.m_mipLodBias;
	createInfoVk.anisotropyEnable = createInfo.m_anisotropyEnable;
	createInfoVk.maxAnisotropy = createInfo.m_maxAnisotropy;
	createInfoVk.compareEnable = createInfo.m_compareEnable;
	createInfoVk.compareOp = static_cast<VkCompareOp>(createInfo.m_compareOp);
	createInfoVk.minLod = createInfo.m_minLod;
	createInfoVk.maxLod = createInfo.m_maxLod;
	createInfoVk.borderColor = static_cast<VkBorderColor>(createInfo.m_borderColor);
	createInfoVk.unnormalizedCoordinates = createInfo.m_unnormalizedCoordinates;

	UtilityVk::checkResult(vkCreateSampler(m_device, &createInfoVk, nullptr, &m_sampler), "Failed to create Sampler!");
}

VEngine::gal::SamplerVk::~SamplerVk()
{
	vkDestroySampler(m_device, m_sampler, nullptr);
}

void *VEngine::gal::SamplerVk::getNativeHandle() const
{
	return m_sampler;
}

VEngine::gal::ImageVk::ImageVk(VkImage image, void *allocHandle, const ImageCreateInfo &createInfo)
	:m_image(image),
	m_allocHandle(allocHandle),
	m_description(createInfo)
{
}

void *VEngine::gal::ImageVk::getNativeHandle() const
{
	return m_image;
}

const VEngine::gal::ImageCreateInfo &VEngine::gal::ImageVk::getDescription() const
{
	return m_description;
}

void *VEngine::gal::ImageVk::getAllocationHandle()
{
	return m_allocHandle;
}

VEngine::gal::BufferVk::BufferVk(VkBuffer buffer, void *allocHandle, const BufferCreateInfo &createInfo, MemoryAllocatorVk *allocator, GraphicsDeviceVk *device)
	:m_buffer(buffer),
	m_allocHandle(allocHandle),
	m_description(createInfo),
	m_allocator(allocator),
	m_device(device)
{
}

void *VEngine::gal::BufferVk::getNativeHandle() const
{
	return m_buffer;
}

const VEngine::gal::BufferCreateInfo &VEngine::gal::BufferVk::getDescription() const
{
	return m_description;
}

void VEngine::gal::BufferVk::map(void **data)
{
	m_allocator->mapMemory(static_cast<AllocationHandleVk>(m_allocHandle), data);
}

void VEngine::gal::BufferVk::unmap()
{
	m_allocator->unmapMemory(static_cast<AllocationHandleVk>(m_allocHandle));
}

void VEngine::gal::BufferVk::invalidate(uint32_t count, const MemoryRange *ranges)
{
	const auto allocInfo = m_allocator->getAllocationInfo(static_cast<AllocationHandleVk>(m_allocHandle));
	const uint64_t nonCoherentAtomSize = m_device->getDeviceProperties().limits.nonCoherentAtomSize;

	constexpr uint32_t batchSize = 16;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkMappedMemoryRange rangesVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &range = ranges[i * batchSize + j];
			rangesVk[j] = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, allocInfo.m_memory, allocInfo.m_offset + range.m_offset, range.m_size };
			rangesVk[j].offset = Utility::alignDown(rangesVk[j].offset, nonCoherentAtomSize);
			rangesVk[j].size = Utility::alignUp(rangesVk[j].size, nonCoherentAtomSize);
		}
		UtilityVk::checkResult(vkInvalidateMappedMemoryRanges(m_device->getDevice(), countVk, rangesVk), "Failed to invalidate mapped memory ranges!");
	}
}

void VEngine::gal::BufferVk::flush(uint32_t count, const MemoryRange *ranges)
{
	const auto allocInfo = m_allocator->getAllocationInfo(static_cast<AllocationHandleVk>(m_allocHandle));
	const uint64_t nonCoherentAtomSize = m_device->getDeviceProperties().limits.nonCoherentAtomSize;

	constexpr uint32_t batchSize = 16;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkMappedMemoryRange rangesVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &range = ranges[i * batchSize + j];
			rangesVk[j] = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, allocInfo.m_memory, allocInfo.m_offset + range.m_offset, range.m_size };
			rangesVk[j].offset = Utility::alignDown(rangesVk[j].offset, nonCoherentAtomSize);
			rangesVk[j].size = Utility::alignUp(rangesVk[j].size, nonCoherentAtomSize);
		}
		UtilityVk::checkResult(vkFlushMappedMemoryRanges(m_device->getDevice(), countVk, rangesVk), "Failed to invalidate mapped memory ranges!");
	}
}

void *VEngine::gal::BufferVk::getAllocationHandle()
{
	return m_allocHandle;
}

VkDeviceMemory VEngine::gal::BufferVk::getMemory() const
{
	return m_allocator->getAllocationInfo(static_cast<AllocationHandleVk>(m_allocHandle)).m_memory;
}

VkDeviceSize VEngine::gal::BufferVk::getOffset() const
{
	return m_allocator->getAllocationInfo(static_cast<AllocationHandleVk>(m_allocHandle)).m_offset;
}

VEngine::gal::ImageViewVk::ImageViewVk(VkDevice device, const ImageViewCreateInfo &createInfo)
	:m_device(device),
	m_imageView(VK_NULL_HANDLE),
	m_description(createInfo)
{
	const auto *imageVk = dynamic_cast<const ImageVk *>(createInfo.m_image);
	assert(imageVk);

	VkImageViewCreateInfo createInfoVk{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	createInfoVk.image = (VkImage)imageVk->getNativeHandle();
	createInfoVk.viewType = static_cast<VkImageViewType>(createInfo.m_viewType);
	createInfoVk.format = static_cast<VkFormat>(createInfo.m_format == Format::UNDEFINED ? imageVk->getDescription().m_format : createInfo.m_format);
	createInfoVk.components = *reinterpret_cast<const VkComponentMapping *>(&createInfo.m_components);
	createInfoVk.subresourceRange =
	{
		UtilityVk::getImageAspectMask(static_cast<VkFormat>(imageVk->getDescription().m_format)),
		createInfo.m_baseMipLevel,
		createInfo.m_levelCount,
		createInfo.m_baseArrayLayer,
		createInfo.m_layerCount
	};

	UtilityVk::checkResult(vkCreateImageView(m_device, &createInfoVk, nullptr, &m_imageView), "Failed to create Image View!");
}

VEngine::gal::ImageViewVk::~ImageViewVk()
{
	vkDestroyImageView(m_device, m_imageView, nullptr);
}

void *VEngine::gal::ImageViewVk::getNativeHandle() const
{
	return m_imageView;
}

const VEngine::gal::Image *VEngine::gal::ImageViewVk::getImage() const
{
	return m_description.m_image;
}

const VEngine::gal::ImageViewCreateInfo &VEngine::gal::ImageViewVk::getDescription() const
{
	return m_description;
}

VEngine::gal::BufferViewVk::BufferViewVk(VkDevice device, const BufferViewCreateInfo &createInfo)
	:m_device(device),
	m_bufferView(VK_NULL_HANDLE),
	m_description(createInfo)
{
	const auto *bufferVk = dynamic_cast<const BufferVk *>(createInfo.m_buffer);
	assert(bufferVk);

	VkBufferViewCreateInfo createInfoVk{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	createInfoVk.buffer = (VkBuffer)bufferVk->getNativeHandle();
	createInfoVk.format = static_cast<VkFormat>(createInfo.m_format);
	createInfoVk.offset = createInfo.m_offset;
	createInfoVk.range = createInfo.m_range;

	UtilityVk::checkResult(vkCreateBufferView(m_device, &createInfoVk, nullptr, &m_bufferView), "Failed to create Buffer View!");
}

VEngine::gal::BufferViewVk::~BufferViewVk()
{
	vkDestroyBufferView(m_device, m_bufferView, nullptr);
}

void *VEngine::gal::BufferViewVk::getNativeHandle() const
{
	return m_bufferView;
}

const VEngine::gal::Buffer *VEngine::gal::BufferViewVk::getBuffer() const
{
	return m_description.m_buffer;
}

const VEngine::gal::BufferViewCreateInfo &VEngine::gal::BufferViewVk::getDescription() const
{
	return m_description;
}
