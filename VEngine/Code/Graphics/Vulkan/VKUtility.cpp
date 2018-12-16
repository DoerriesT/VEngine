#include "VKUtility.h"
#include "Utility/Utility.h"
#include "VKContext.h"
#include "VKImageData.h"

void VEngine::VKUtility::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory) 
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = numSamples;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkImageFormatProperties props;
	if (vkGetPhysicalDeviceImageFormatProperties(g_context.m_physicalDevice, imageInfo.format, imageInfo.imageType, imageInfo.tiling, imageInfo.usage, imageInfo.flags, &props) != VK_SUCCESS)
	{
		VkFormatProperties formatProps;
		vkGetPhysicalDeviceFormatProperties(g_context.m_physicalDevice, imageInfo.format, &formatProps);
		Utility::fatalExit("Requested image format not supported!", -1);
	}

	if (vkCreateImage(g_context.m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) 
	{
		Utility::fatalExit("Failed to create image!", -1);
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(g_context.m_device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(g_context.m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) 
	{
		Utility::fatalExit("Failed to allocate image memory!", -1);
	}

	vkBindImageMemory(g_context.m_device, image, imageMemory, 0);
}

void VEngine::VKUtility::createImageView(const VkImageSubresourceRange &subresourceRange, VKImageData &image)
{
	VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = image.m_image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = image.m_format;
	viewInfo.subresourceRange = subresourceRange;

	if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &image.m_view) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create image view!", -1);
	}
}

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

void VEngine::VKUtility::copyBuffer(VkQueue queue, VkCommandPool commandPool, VKBufferData srcBuffer, VKBufferData dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool);

	VkBufferCopy copyRegion = { 0, 0, size };
	vkCmdCopyBuffer(commandBuffer, srcBuffer.m_buffer, dstBuffer.m_buffer, 1, &copyRegion);

	endSingleTimeCommands(queue, commandPool, commandBuffer);
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

void VEngine::VKUtility::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory, VkDeviceSize &allocationSize)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(g_context.m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create buffer!", -1);
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(g_context.m_device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	allocationSize = allocInfo.allocationSize;

	if (vkAllocateMemory(g_context.m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to allocate buffer memory!", -1);
	}

	vkBindBufferMemory(g_context.m_device, buffer, bufferMemory, 0);
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
