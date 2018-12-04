#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "VKBufferData.h"

namespace VEngine
{
	struct VKImageData;

	namespace VKUtility
	{
		void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory);
		void createImageView(const VkImageSubresourceRange &subresourceRange, VKImageData &image);
		VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);
		void endSingleTimeCommands(VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer);
		void copyBuffer(VkQueue queue, VkCommandPool commandPool, VKBufferData srcBuffer, VKBufferData dstBuffer, VkDeviceSize size);
		void setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, const VkImageSubresourceRange &subresourceRange, VkPipelineStageFlags srcStages, VkPipelineStageFlags destStages);
		void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize &allocationSize);
		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	}
}