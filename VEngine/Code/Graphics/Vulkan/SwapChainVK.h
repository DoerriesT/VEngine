#pragma once
#include "Graphics/Vulkan/volk.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	class SwapChainVK
	{
	public:
		explicit SwapChainVK(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, unsigned int width, unsigned int height, VkQueue presentQueue);
		SwapChainVK(SwapChainVK &) = delete;
		SwapChainVK(SwapChainVK &&) = delete;
		SwapChainVK &operator=(const SwapChainVK &) = delete;
		SwapChainVK &operator=(const SwapChainVK &&) = delete;
		~SwapChainVK();
		void recreate(unsigned int width, unsigned int height);
		void getCurrentImageIndex(uint32_t frameIndex, uint32_t &currentImageIndex, VkSemaphore &swapChainAvailableSemaphore);
		void present(uint32_t waitSemaphoreCount, VkSemaphore *waitSemaphores);
		VkExtent2D getExtent() const;
		VkFormat getImageFormat() const;
		VkImage getImage(size_t index) const;
		VkQueue getPresentQueue() const;

	private:
		static constexpr uint32_t s_maxImageCount = 8;

		VkPhysicalDevice m_physicalDevice;
		VkDevice m_device;
		VkSurfaceKHR m_surface;
		VkQueue m_presentQueue;
		VkSwapchainKHR m_swapChain;
		uint32_t m_imageCount;
		VkImage m_images[s_maxImageCount];
		VkFormat m_imageFormat;
		VkExtent2D m_extent;
		uint32_t m_currentImageIndex;
		bool m_currentImageIndexStale;
		VkSemaphore m_semaphores[RendererConsts::FRAMES_IN_FLIGHT];

		void create(unsigned int width, unsigned int height);
		void destroy();
	};
}