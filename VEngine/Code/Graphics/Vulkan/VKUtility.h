#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	namespace VKUtility
	{
		VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);
		void endSingleTimeCommands(VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer);
		void setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, const VkImageSubresourceRange &subresourceRange, VkPipelineStageFlags srcStages, VkPipelineStageFlags destStages);
		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		bool dispatchComputeHelper(VkCommandBuffer commandBuffer, uint32_t domainX, uint32_t domainY, uint32_t domainZ, uint32_t localSizeX, uint32_t localSizeY, uint32_t localSizeZ);
		bool isDepthFormat(VkFormat format);
		bool isStencilFormat(VkFormat format);
		VkImageAspectFlags imageAspectMaskFromFormat(VkFormat format);
		VkDeviceSize align(VkDeviceSize in, VkDeviceSize alignment);
	}
}