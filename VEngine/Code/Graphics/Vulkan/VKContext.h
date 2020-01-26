#pragma once
#include "Graphics/Vulkan/volk.h"
#include <vector>
#include "VKMemoryAllocator.h"
#include "SyncPrimitivePool.h"

struct GLFWwindow;

namespace VEngine
{
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
		VkSurfaceKHR m_surface;
		VKQueueFamilyIndices m_queueFamilyIndices;
		VKMemoryAllocator m_allocator;
		SyncPrimitivePool m_syncPrimitivePool;
		VkDebugUtilsMessengerEXT m_debugUtilsMessenger;

		void init(GLFWwindow *windowHandle);
		void shutdown();
	};

	extern VKContext g_context;
}