#pragma once
#include <cstdint>
#include "Graphics/Vulkan/volk.h"
#include "VKMemoryAllocator.h"

namespace VEngine
{
	class VKImage
	{
	public:
		VKImage();
		void create(const VkImageCreateInfo &imageCreateInfo, const VKAllocationCreateInfo &allocCreateInfo);
		void destroy();
		VkImage getImage() const;
		VkImageType getImageType() const;
		VkFormat getFormat() const;
		uint32_t getWidth() const;
		uint32_t getHeight() const;
		uint32_t getDepth() const;
		uint32_t getMipLevels() const;
		uint32_t getArrayLayers() const;
		VkSampleCountFlagBits getSamples() const;
		VkImageTiling getTiling() const;
		VkSharingMode getSharingMode() const;
		VKAllocationHandle getAllocation() const;
		VkDeviceMemory getDeviceMemory() const;
		VkDeviceSize getOffset() const;
		bool isValid() const;

	private:
		VkImage m_image;
		VkImageType m_imageType;
		VkFormat m_format;
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_depth;
		uint32_t m_mipLevels;
		uint32_t m_arrayLayers;
		VkSampleCountFlagBits m_samples;
		VkImageTiling m_tiling;
		VkSharingMode m_sharingMode;
		VKAllocationHandle m_allocation;
		VkDeviceMemory m_deviceMemory;
		VkDeviceSize m_offset;
		bool m_valid;
	};
}