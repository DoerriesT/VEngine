#include "VKUtility.h"
#include "Utility/Utility.h"
#include "VKContext.h"

VkFormat VEngine::VKUtility::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(g_context.m_physicalDevice, format, &props);

		if ((tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			|| (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features))
		{
			return format;
		}
	}

	Utility::fatalExit("Failed to find supported format!", -1);
	return VkFormat();
}

VkCommandBuffer VEngine::VKUtility::beginSingleTimeCommands(VkCommandPool commandPool)
{
	VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(g_context.m_device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VEngine::VKUtility::endSingleTimeCommands(VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(g_context.m_device, commandPool, 1, &commandBuffer);
}

void VEngine::VKUtility::setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, const VkImageSubresourceRange &subresourceRange, VkPipelineStageFlags srcStages, VkPipelineStageFlags destStages)
{
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;
	barrier.oldLayout = oldImageLayout;
	barrier.newLayout = newImageLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = subresourceRange;
	//barrier.subresourceRange.aspectMask = aspectMask;
	//barrier.subresourceRange.baseMipLevel = 0;
	//barrier.subresourceRange.levelCount = 1;
	//barrier.subresourceRange.baseArrayLayer = 0;
	//barrier.subresourceRange.layerCount = 1;

	switch (oldImageLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		barrier.srcAccessMask = 0;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;

	default:
		break;
	}

	switch (newImageLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	default:
		break;
	}

	vkCmdPipelineBarrier(commandBuffer, srcStages, destStages, 0, 0, NULL, 0, NULL, 1, &barrier);
}

uint32_t VEngine::VKUtility::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(g_context.m_physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	Utility::fatalExit("Failed to find suitable memory type!", -1);
	return 0;
}

bool VEngine::VKUtility::dispatchComputeHelper(VkCommandBuffer commandBuffer, uint32_t domainX, uint32_t domainY, uint32_t domainZ, uint32_t localSizeX, uint32_t localSizeY, uint32_t localSizeZ)
{
	uint32_t numGroupsX = domainX / localSizeX + ((domainX % localSizeX == 0) ? 0 : 1);
	uint32_t numGroupsY = domainY / localSizeY + ((domainY % localSizeY == 0) ? 0 : 1);
	uint32_t numGroupsZ = domainZ / localSizeZ + ((domainZ % localSizeZ == 0) ? 0 : 1);
	vkCmdDispatch(commandBuffer, numGroupsX, numGroupsY, numGroupsZ);
	return (domainX % localSizeX == 0) && (domainY % localSizeY == 0) && (domainZ % localSizeZ == 0);
}

VkImageAspectFlags VEngine::VKUtility::imageAspectMaskFromFormat(VkFormat format)
{
	bool isDepthFormat = false;
	bool isStencilFormat = false;

	switch (format)
	{
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D32_SFLOAT:
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		isDepthFormat = true;
		break;
	default:
		break;
	}

	switch (format)
	{
	case VK_FORMAT_S8_UINT:
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		isStencilFormat = true;
		break;
	default:
		break;
	}

	VkImageAspectFlags mask = 0;

	mask |= isDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
	mask |= isStencilFormat ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
	mask |= (!isDepthFormat && !isStencilFormat) ? VK_IMAGE_ASPECT_COLOR_BIT : 0;

	return mask;
}

VkDeviceSize VEngine::VKUtility::align(VkDeviceSize in, VkDeviceSize alignment)
{
	if (alignment > 0)
	{
		in = (in + alignment - 1) & ~(alignment - 1);
	}
	return in;
}
