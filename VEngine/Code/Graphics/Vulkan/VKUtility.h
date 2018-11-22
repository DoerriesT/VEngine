#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "VKBufferData.h"

namespace VEngine
{
	struct VKImageData;

	namespace VKUtility
	{
		void createImage(uint32_t width, uint32_t height, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VKImageData &image);
		void createImageView(VkImageAspectFlags aspectFlags, VKImageData &image);
		VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);
		void endSingleTimeCommands(VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer);
		void copyBuffer(VkQueue queue, VkCommandPool commandPool, VKBufferData srcBuffer, VKBufferData dstBuffer, VkDeviceSize size);
		void setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStages, VkPipelineStageFlags destStages);
	}
}