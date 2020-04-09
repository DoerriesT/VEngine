#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"
#include "ResourceVk.h"
#include "Utility/ObjectPool.h"

namespace VEngine
{
	namespace gal
	{
		class SwapChainVk2 : public SwapChain
		{
		public:
			explicit SwapChainVk2(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, Queue *presentQueue, uint32_t width, uint32_t height);
			SwapChainVk2(SwapChainVk2 &) = delete;
			SwapChainVk2(SwapChainVk2 &&) = delete;
			SwapChainVk2 &operator=(const SwapChainVk2 &) = delete;
			SwapChainVk2 &operator=(const SwapChainVk2 &&) = delete;
			~SwapChainVk2();
			void *getNativeHandle() const override;
			void resize(uint32_t width, uint32_t height) override;
			void getCurrentImageIndex(uint32_t &currentImageIndex, Semaphore *signalSemaphore, uint64_t semaphoreSignalValue) override;
			void present(Semaphore *waitSemaphore, uint64_t semaphoreWaitValue) override;
			Extent2D getExtent() const override;
			Extent2D getRecreationExtent() const override;
			Format getImageFormat() const override;
			Image *getImage(size_t index) const override;
			Queue *getPresentQueue() const override;
			void setFrameIndex(uint64_t frameIndex);

		private:
			static constexpr uint32_t s_maxImageCount = 8;
			static constexpr uint32_t s_semaphoreCount = 3;

			VkPhysicalDevice m_physicalDevice;
			VkDevice m_device;
			VkSurfaceKHR m_surface;
			Queue *m_presentQueue;
			VkSwapchainKHR m_swapChain;
			uint32_t m_imageCount;
			ImageVk *m_images[s_maxImageCount];
			StaticObjectPool<ByteArray<sizeof(ImageVk)>, s_maxImageCount> m_imageMemoryPool;
			Format m_imageFormat;
			Extent2D m_extent;
			uint32_t m_currentImageIndex;
			bool m_currentImageIndexStale;
			VkSemaphore m_acquireSemaphores[s_semaphoreCount];
			VkSemaphore m_presentSemaphores[s_semaphoreCount];
			uint64_t m_frameIndex;

			void create(uint32_t width, uint32_t height);
			void destroy();
		};
	}
}