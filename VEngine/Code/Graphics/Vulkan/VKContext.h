#pragma once
#include <vulkan\vulkan.h>
#include <vector>

#ifndef ENABLE_VALIDATION_LAYERS
#define ENABLE_VALIDATION_LAYERS 1
#endif // ENABLE_VALIDATION_LAYERS

struct GLFWwindow;

namespace VEngine
{
	struct VKSwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR m_capabilities;
		std::vector<VkSurfaceFormatKHR> m_formats;
		std::vector<VkPresentModeKHR> m_presentModes;
	};

	struct VKQueueFamilyIndices
	{
		uint32_t m_graphicsFamily;
		uint32_t m_presentFamily;
		uint32_t m_computeFamily;
		uint32_t m_transferFamily;
	};

	struct VKContext
	{
		VkInstance m_instance;
		VkDevice m_device;
		VkPhysicalDevice m_physicalDevice;
		VkQueue m_graphicsQueue;
		VkQueue m_presentQueue;
		VkQueue m_computeQueue;
		VkQueue m_transferQueue;
		VkCommandPool m_graphicsCommandPool;
		VkCommandPool m_computeCommandPool;
		VkSurfaceKHR m_surface;
		VkDebugReportCallbackEXT m_debugCallback;
		VkSemaphore m_imageAvailableSemaphore;
		VkSemaphore m_renderFinishedSemaphore;
		VKSwapChainSupportDetails m_swapChainSupportDetails;
		VKQueueFamilyIndices m_queueFamilyIndices;

		void init(GLFWwindow *windowHandle);
		void shutdown();
	};
}