#include "VKSwapChain.h"
#include <cassert>
#include <algorithm>
#include "VKContext.h"
#include "Utility/Utility.h"
#include "VKUtility.h"

extern VEngine::VKContext g_context;

VEngine::VKSwapChain::VKSwapChain()
{
}

VEngine::VKSwapChain::~VKSwapChain()
{
	shutdown();
}

void VEngine::VKSwapChain::init(unsigned int width, unsigned int height)
{
	// find surface format
	VkSurfaceFormatKHR surfaceFormat;
	{
		auto &formats = g_context.m_swapChainSupportDetails.m_formats;
		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		{
			surfaceFormat = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}
		else
		{
			bool foundOptimal = false;
			for (const auto& format : formats)
			{
				if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				{
					surfaceFormat = format;
					foundOptimal = true;
					break;
				}
			}
			if (!foundOptimal)
			{
				surfaceFormat = formats[0];
			}
		}
	}

	// find present mode
	VkPresentModeKHR presentMode;
	{
		auto &presentModes = g_context.m_swapChainSupportDetails.m_presentModes;

		VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

		bool foundOptimal = false;
		for (const auto& mode : presentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				presentMode = mode;
				foundOptimal = true;
				break;
			}
			else if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				bestMode = mode;
			}
		}
		if (!foundOptimal)
		{
			presentMode = bestMode;
		}
	}

	auto &caps = g_context.m_swapChainSupportDetails.m_capabilities;

	// find proper extent
	VkExtent2D extent;
	{
		

		if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			extent = caps.currentExtent;
		}
		else
		{
			extent = 
			{ 
				std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, width)),
				std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, height))
			};
		}
	}

	uint32_t imageCount = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
	{
		imageCount = caps.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	createInfo.surface = g_context.m_surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

	
	if (g_context.m_queueFamilyIndices.m_graphicsFamily != g_context.m_queueFamilyIndices.m_presentFamily)
	{
		uint32_t queueFamilyIndices[] = { g_context.m_queueFamilyIndices.m_graphicsFamily , g_context.m_queueFamilyIndices.m_presentFamily };

		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = caps.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(g_context.m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create swap chain!", -1);
	}

	vkGetSwapchainImagesKHR(g_context.m_device, m_swapChain, &imageCount, nullptr);
	m_images.resize(imageCount);
	vkGetSwapchainImagesKHR(g_context.m_device, m_swapChain, &imageCount, m_images.data());

	m_imageFormat = surfaceFormat.format;
	m_extent = extent;

	m_imageViews.resize(m_images.size());


	for (size_t i = 0; i < m_images.size(); i++)
	{
		VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = m_images[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_imageFormat;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image view!", -1);
		}
	}

	VkSurfaceCapabilitiesKHR surfCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_context.m_physicalDevice, g_context.m_surface, &surfCapabilities);
	if (!(surfCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) 
	{
		Utility::fatalExit("Surface cannot be destination of blit!", -1);
	}
}

void VEngine::VKSwapChain::recreate(unsigned int width, unsigned int height)
{
	vkDeviceWaitIdle(g_context.m_device);
	shutdown();
	init(width, height);
}

VkExtent2D VEngine::VKSwapChain::getExtent() const
{
	return m_extent;
}

VkFormat VEngine::VKSwapChain::getImageFormat() const
{
	return m_imageFormat;
}

VkSwapchainKHR VEngine::VKSwapChain::get() const
{
	return m_swapChain;
}

VkImage VEngine::VKSwapChain::getImage(size_t index) const
{
	assert(index < m_images.size());
	return m_images[index];
}

std::size_t VEngine::VKSwapChain::getImageCount() const
{
	return m_images.size();
}

void VEngine::VKSwapChain::shutdown()
{
	for (auto imageView : m_imageViews)
	{
		vkDestroyImageView(g_context.m_device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(g_context.m_device, m_swapChain, nullptr);
}
