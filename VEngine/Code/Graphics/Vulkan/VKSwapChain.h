#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace VEngine
{
	class VKSwapChain
	{
	public:
		explicit VKSwapChain();
		VKSwapChain(VKSwapChain &) = delete;
		VKSwapChain(VKSwapChain &&) = delete;
		VKSwapChain &operator= (const VKSwapChain &) = delete;
		VKSwapChain &operator= (const VKSwapChain &&) = delete;
		~VKSwapChain();
		void init(unsigned int width, unsigned int height);
		void recreate(unsigned int width, unsigned int height);
		VkExtent2D getExtent() const;
		VkFormat getImageFormat() const;
		VkSwapchainKHR get() const;
		VkImage getImage(size_t index) const;
		size_t getImageCount() const;

	private:
		VkSwapchainKHR m_swapChain;
		std::vector<VkImage> m_images;
		VkFormat m_imageFormat;
		VkExtent2D m_extent;
		std::vector<VkImageView> m_imageViews;

		void cleanup();
	};
}