#include "SwapChainVk2.h"
#include <limits>
#include "Utility/Utility.h"
#include "UtilityVk.h"
#include "SemaphoreVk.h"
#include "QueueVk.h"
#include <vector>
#include <algorithm>

VEngine::gal::SwapChainVk2::SwapChainVk2(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, Queue *presentQueue, uint32_t width, uint32_t height)
	:m_physicalDevice(physicalDevice),
	m_device(device),
	m_surface(surface),
	m_presentQueue(presentQueue),
	m_swapChain(VK_NULL_HANDLE),
	m_imageCount(),
	m_images(),
	m_imageMemoryPool(),
	m_imageFormat(),
	m_extent(),
	m_currentImageIndex(),
	m_currentImageIndexStale(true),
	m_acquireSemaphores(),
	m_presentSemaphores(),
	m_frameIndex(uint64_t() - 1)
{
	create(width, height);
}

VEngine::gal::SwapChainVk2::~SwapChainVk2()
{
	destroy();
}

void *VEngine::gal::SwapChainVk2::getNativeHandle() const
{
	return m_swapChain;
}

void VEngine::gal::SwapChainVk2::resize(uint32_t width, uint32_t height)
{
	vkDeviceWaitIdle(m_device);
	destroy();
	create(width, height);
}

void VEngine::gal::SwapChainVk2::getCurrentImageIndex(uint32_t &currentImageIndex, Semaphore *signalSemaphore, uint64_t semaphoreSignalValue)
{
	const uint32_t resIdx = m_frameIndex % 2;

	if (m_currentImageIndexStale)
	{
		m_currentImageIndexStale = false;

		// try to acquire the next image index.
		bool tryAgain = false;
		int remainingAttempts = 3;
		do
		{
			--remainingAttempts;
			VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, std::numeric_limits<uint64_t>::max(), m_acquireSemaphores[resIdx], VK_NULL_HANDLE, &m_currentImageIndex);

			switch (result)
			{
			case VK_SUCCESS:
				break;
			case VK_SUBOPTIMAL_KHR:
			case VK_ERROR_OUT_OF_DATE_KHR:
				resize(m_extent.m_width, m_extent.m_height);
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

	// Vulkan swapchains do not support timeline semaphores, so we need to wait on a binary semaphore and signal the timeline semaphore
	{
		VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkSemaphore signalSemaphoreVk = (VkSemaphore)signalSemaphore->getNativeHandle();

		uint64_t dummyValue = 0;

		VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
		timelineSubmitInfo.waitSemaphoreValueCount = 1;
		timelineSubmitInfo.pWaitSemaphoreValues = &dummyValue;
		timelineSubmitInfo.signalSemaphoreValueCount = 1;
		timelineSubmitInfo.pSignalSemaphoreValues = &semaphoreSignalValue;

		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.pNext = &timelineSubmitInfo;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &m_acquireSemaphores[resIdx];
		submitInfo.pWaitDstStageMask = &waitDstStageMask;
		submitInfo.commandBufferCount = 0;
		submitInfo.pCommandBuffers = nullptr;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &signalSemaphoreVk;

		UtilityVk::checkResult(vkQueueSubmit((VkQueue)m_presentQueue->getNativeHandle(), 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit to Queue!");
	}
}

void VEngine::gal::SwapChainVk2::present(Semaphore *waitSemaphore, uint64_t semaphoreWaitValue)
{
	const uint32_t resIdx = m_frameIndex % 2;

	// Vulkan swapchains do not support timeline semaphores, so we need to wait on the timeline semaphore and signal the binary semaphore
	{
		VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkSemaphore waitSemaphoreVk = (VkSemaphore)waitSemaphore->getNativeHandle();

		uint64_t dummyValue = 0;

		VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{ VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
		timelineSubmitInfo.waitSemaphoreValueCount = 1;
		timelineSubmitInfo.pWaitSemaphoreValues = &semaphoreWaitValue;
		timelineSubmitInfo.signalSemaphoreValueCount = 1;
		timelineSubmitInfo.pSignalSemaphoreValues = &dummyValue;

		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO, &timelineSubmitInfo };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &waitSemaphoreVk;
		submitInfo.pWaitDstStageMask = &waitDstStageMask;
		submitInfo.commandBufferCount = 0;
		submitInfo.pCommandBuffers = nullptr;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_presentSemaphores[resIdx];

		UtilityVk::checkResult(vkQueueSubmit((VkQueue)m_presentQueue->getNativeHandle(), 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit to Queue!");
	}
	
	VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_presentSemaphores[resIdx];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain;
	presentInfo.pImageIndices = &m_currentImageIndex;

	UtilityVk::checkResult(vkQueuePresentKHR((VkQueue)m_presentQueue->getNativeHandle(), &presentInfo), "Failed to present!");

	m_currentImageIndexStale = true;
}

VEngine::gal::Extent2D VEngine::gal::SwapChainVk2::getExtent() const
{
	return m_extent;
}

VEngine::gal::Extent2D VEngine::gal::SwapChainVk2::getRecreationExtent() const
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities);

	const auto extentVk = surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() ? surfaceCapabilities.currentExtent : surfaceCapabilities.minImageExtent;
	return { extentVk.width, extentVk.height };
}

VEngine::gal::Format VEngine::gal::SwapChainVk2::getImageFormat() const
{
	return m_imageFormat;
}

VEngine::gal::Image *VEngine::gal::SwapChainVk2::getImage(size_t index) const
{
	assert(index < m_imageCount);
	return m_images[index];
}

VEngine::gal::Queue *VEngine::gal::SwapChainVk2::getPresentQueue() const
{
	return m_presentQueue;
}

void VEngine::gal::SwapChainVk2::setFrameIndex(uint64_t frameIndex)
{
	m_frameIndex = frameIndex;
}

void VEngine::gal::SwapChainVk2::create(uint32_t width, uint32_t height)
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

	VkImage imagesVk[s_maxImageCount];

	vkGetSwapchainImagesKHR(m_device, m_swapChain, &m_imageCount, imagesVk);

	ImageCreateInfo imageCreateInfo{};
	imageCreateInfo.m_width = extent.width;
	imageCreateInfo.m_height = extent.height;
	imageCreateInfo.m_depth = 1;
	imageCreateInfo.m_layers = 1;
	imageCreateInfo.m_levels = 1;
	imageCreateInfo.m_samples = SampleCount::_1;
	imageCreateInfo.m_imageType = ImageType::_2D;
	imageCreateInfo.m_format = static_cast<Format>(surfaceFormat.format);
	imageCreateInfo.m_createFlags = 0;
	imageCreateInfo.m_usageFlags = createInfo.imageUsage;

	for (uint32_t i = 0; i < m_imageCount; ++i)
	{
		auto *memory = m_imageMemoryPool.alloc();
		assert(memory);

		m_images[i] = new(memory) ImageVk(imagesVk[i], nullptr, imageCreateInfo);
	}

	m_imageFormat = static_cast<Format>(surfaceFormat.format);
	m_extent = { extent.width, extent.height };

	// create semaphores
	for (size_t i = 0; i < s_semaphoreCount; ++i)
	{
		VkSemaphoreCreateInfo createInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		UtilityVk::checkResult(vkCreateSemaphore(m_device, &createInfo, nullptr, &m_acquireSemaphores[i]), "Failed to create semaphore!");
		UtilityVk::checkResult(vkCreateSemaphore(m_device, &createInfo, nullptr, &m_presentSemaphores[i]), "Failed to create semaphore!");
	}
}

void VEngine::gal::SwapChainVk2::destroy()
{
	vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
	for (size_t i = 0; i < s_semaphoreCount; ++i)
	{
		vkDestroySemaphore(m_device, m_acquireSemaphores[i], nullptr);
		vkDestroySemaphore(m_device, m_presentSemaphores[i], nullptr);
	}

	for (uint32_t i = 0; i < m_imageCount; ++i)
	{
		// call destructor and free backing memory
		m_images[i]->~ImageVk();
		m_imageMemoryPool.free(reinterpret_cast<ByteArray<sizeof(ImageVk)> *>(m_images[i]));
	}
}
