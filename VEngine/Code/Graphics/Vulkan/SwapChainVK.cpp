#include "SwapChainVK.h"
#include <vector>
#include <algorithm>
#include "Utility/Utility.h"

VEngine::SwapChainVK::SwapChainVK(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, unsigned int width, unsigned int height, VkQueue presentQueue)
	:m_physicalDevice(physicalDevice),
	m_device(device),
	m_surface(surface),
	m_presentQueue(presentQueue),
	m_swapChain(),
	m_imageCount(),
	m_imageFormat(),
	m_extent({ width, height }),
	m_currentImageIndex(),
	m_currentImageIndexStale(true)
{
	create(width, height);
}

VEngine::SwapChainVK::~SwapChainVK()
{
	destroy();
}

void VEngine::SwapChainVK::recreate(unsigned int width, unsigned int height)
{
	vkDeviceWaitIdle(m_device);
	destroy();
	create(width, height);
}

void VEngine::SwapChainVK::getCurrentImageIndex(uint32_t frameIndex, uint32_t &currentImageIndex, VkSemaphore &swapChainAvailableSemaphore)
{
	const uint32_t resIdx = frameIndex % RendererConsts::FRAMES_IN_FLIGHT;

	if (m_currentImageIndexStale)
	{
		m_currentImageIndexStale = false;

		// try to acquire the next image index.
		bool tryAgain = false;
		int remainingAttempts = 3;
		do
		{
			--remainingAttempts;
			VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, std::numeric_limits<uint64_t>::max(), m_semaphores[resIdx], VK_NULL_HANDLE, &m_currentImageIndex);

			switch (result)
			{
			case VK_SUCCESS:
				break;
			case VK_SUBOPTIMAL_KHR:
			case VK_ERROR_OUT_OF_DATE_KHR:
				recreate(m_extent.width, m_extent.height);
				tryAgain = true;
				break;
			case VK_ERROR_SURFACE_LOST_KHR:
				Utility::fatalExit("Failed to acquire swap chain image! VK_ERROR_SURFACE_LOST_KHR", EXIT_FAILURE);
				break;

			default:
				break;
			}
		} while (tryAgain && remainingAttempts > 0);

		if (remainingAttempts <= 0)
		{
			Utility::fatalExit("Failed to acquire swap chain image! Too many failed attempts at swapchain recreation.", EXIT_FAILURE);
		}
	}

	currentImageIndex = m_currentImageIndex;
	swapChainAvailableSemaphore = m_semaphores[resIdx];
}

void VEngine::SwapChainVK::present(uint32_t waitSemaphoreCount, VkSemaphore *waitSemaphores)
{
	VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = waitSemaphoreCount;
	presentInfo.pWaitSemaphores = waitSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain;
	presentInfo.pImageIndices = &m_currentImageIndex;

	if (vkQueuePresentKHR(m_presentQueue, &presentInfo) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to present!", EXIT_FAILURE);
	}

	m_currentImageIndexStale = true;
}

VkExtent2D VEngine::SwapChainVK::getExtent() const
{
	return m_extent;
}

VkFormat VEngine::SwapChainVK::getImageFormat() const
{
	return m_imageFormat;
}

VkImage VEngine::SwapChainVK::getImage(size_t index) const
{
	return m_images[index];
}

VkQueue VEngine::SwapChainVK::getPresentQueue() const
{
	return m_presentQueue;
}

void VEngine::SwapChainVK::create(unsigned int width, unsigned int height)
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		formats.resize(static_cast<size_t>(formatCount));
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		presentModes.resize(static_cast<size_t>(presentModeCount));
		vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data());
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
			for (const auto &format : formats)
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
		for (const auto &mode : presentModes)
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

	m_imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && m_imageCount > surfaceCapabilities.maxImageCount)
	{
		m_imageCount = surfaceCapabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	createInfo.surface = m_surface;
	createInfo.minImageCount = m_imageCount;
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

	if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create swap chain!", EXIT_FAILURE);
	}

	vkGetSwapchainImagesKHR(m_device, m_swapChain, &m_imageCount, nullptr);
	if (m_imageCount > s_maxImageCount)
	{
		Utility::fatalExit("Swap chain image count higher than supported maximum!", EXIT_FAILURE);
	}
	vkGetSwapchainImagesKHR(m_device, m_swapChain, &m_imageCount, m_images);

	m_imageFormat = surfaceFormat.format;
	m_extent = extent;

	// create semaphores
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, m_semaphores + i);
	}
}

void VEngine::SwapChainVK::destroy()
{
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(m_device, m_semaphores[i], nullptr);
	}
}
