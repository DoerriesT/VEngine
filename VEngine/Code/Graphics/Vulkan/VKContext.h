#pragma once
#include <vulkan\vulkan.h>
#include <vector>
#include "vk_mem_alloc_include.h"

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
		uint32_t m_computeFamily;
		uint32_t m_transferFamily;
		bool m_graphicsFamilyPresentable;
		bool m_computeFamilyPresentable;
	};

	struct VKContext
	{
		VkInstance m_instance;
		VkDevice m_device;
		VkPhysicalDevice m_physicalDevice;
		VkPhysicalDeviceFeatures m_features;
		VkPhysicalDeviceFeatures m_enabledFeatures;
		VkPhysicalDeviceProperties m_properties;
		VkPhysicalDeviceSubgroupProperties m_subgroupProperties;
		VkQueue m_graphicsQueue;
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
		VmaAllocator m_allocator;

		void init(GLFWwindow *windowHandle);
		void shutdown();
		void querySupportedFormats();
	};

	extern VKContext g_context;
}