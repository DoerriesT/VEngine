#include "VKSwapChain.h"
#include <cassert>
#include <algorithm>
#include "VKContext.h"
#include "Utility/Utility.h"
#include "VKUtility.h"

VEngine::VKSwapChain::VKSwapChain(unsigned int width, unsigned int height)
	:m_swapChain(),
	m_imageFormat(),
	m_extent()
{
	create(width, height);
}

VEngine::VKSwapChain::~VKSwapChain()
{
	shutdown();
}

void VEngine::VKSwapChain::create(unsigned int width, unsigned int height)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_context.m_physicalDevice, g_context.m_surface, &surfaceCapabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(g_context.m_physicalDevice, g_context.m_surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		formats.resize(static_cast<size_t>(formatCount));
		vkGetPhysicalDeviceSurfaceFormatsKHR(g_context.m_physicalDevice, g_context.m_surface, &formatCount, formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(g_context.m_physicalDevice, g_context.m_surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		presentModes.resize(static_cast<size_t>(presentModeCount));
		vkGetPhysicalDeviceSurfacePresentModesKHR(g_context.m_physicalDevice, g_context.m_surface, &presentModeCount, presentModes.data());
	}

	// find surface format
	VkSurfaceFormatKHR surfaceFormat;
	{
		if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		{
			surfaceFormat = { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}
		else
		{
			bool foundOptimal = false;
			for (const auto& format : formats)
			{
				if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
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

	// find proper extent
	VkExtent2D extent;
	{
		if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			extent = surfaceCapabilities.currentExtent;
		}
		else
		{
			extent = 
			{ 
				std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, width)),
				std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, height))
			};
		}
	}

	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
	{
		imageCount = surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	createInfo.surface = g_context.m_surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = surfaceCapabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(g_context.m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create swap chain!", EXIT_FAILURE);
	}

	vkGetSwapchainImagesKHR(g_context.m_device, m_swapChain, &imageCount, nullptr);
	m_images.resize(static_cast<size_t>(imageCount));
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
			Utility::fatalExit("Failed to create image view!", EXIT_FAILURE);
		}
	}
}

void VEngine::VKSwapChain::recreate(unsigned int width, unsigned int height)
{
	vkDeviceWaitIdle(g_context.m_device);
	shutdown();
	create(width, height);
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

VkImageView VEngine::VKSwapChain::getImageView(size_t index) const
{
	return m_imageViews[index];
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
