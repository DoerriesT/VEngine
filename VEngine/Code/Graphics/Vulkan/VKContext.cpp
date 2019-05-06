#include "VKContext.h"
#include "Utility/Utility.h"
#include <iostream>
#include <GLFW/glfw3.h>
#include <set>
#include "GlobalVar.h"

namespace VEngine
{
	VKContext g_context = {};

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
	{
		std::cerr << "validation layer: " << msg << std::endl;

		return VK_FALSE;
	}

	void VKContext::init(GLFWwindow *windowHandle)
	{
		// create instance
		{
			VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
			appInfo.pApplicationName = "Vulkan";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "No Engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_1;

			// extensions
			uint32_t glfwExtensionCount = 0;
			const char** glfwExtensions;
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
			std::vector<const char*> extensions(glfwExtensions, glfwExtensions + static_cast<size_t>(glfwExtensionCount));

			if (g_vulkanDebugCallBackEnabled)
			{
				extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
			}

			VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
			createInfo.pApplicationInfo = &appInfo;
			createInfo.enabledLayerCount = 0;
			createInfo.ppEnabledLayerNames = nullptr;
			createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
			createInfo.ppEnabledExtensionNames = extensions.data();

			if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create instance!", -1);
			}
		}

		// create debug callback
		if (g_vulkanDebugCallBackEnabled)
		{
			VkDebugReportCallbackCreateInfoEXT createInfo{ VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
			createInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT;
			createInfo.pfnCallback = debugCallback;

			auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");
			if (!func || func(m_instance, &createInfo, nullptr, &m_debugCallback) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to set up debug callback!", -1);
			}
		}

		// create window surface
		{
			if (glfwCreateWindowSurface(m_instance, windowHandle, nullptr, &m_surface) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create window surface!", -1);
			}
		}

		const char *const deviceExtensions[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VKSwapChainSupportDetails swapChainSupportDetails;

		// pick physical device
		{
			uint32_t physicalDeviceCount = 0;
			vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);

			if (physicalDeviceCount == 0)
			{
				Utility::fatalExit("Failed to find GPUs with Vulkan support!", -1);
			}

			std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
			vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data());

			for (const auto& physicalDevice : physicalDevices)
			{
				// find queue indices
				int graphicsFamilyIndex = -1;
				int computeFamilyIndex = -1;
				int transferFamilyIndex = -1;
				VkBool32 graphicsFamilyPresentable = VK_FALSE;
				VkBool32 computeFamilyPresentable = VK_FALSE;
				{
					uint32_t queueFamilyCount = 0;
					vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

					std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
					vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

					// find graphics queue
					for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex)
					{
						auto &queueFamily = queueFamilies[queueFamilyIndex];
						if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
						{
							graphicsFamilyIndex = queueFamilyIndex;
						}
					}

					// find compute queue
					for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex)
					{
						// we're trying to find dedicated queues, so skip the graphics queue
						if (queueFamilyIndex == graphicsFamilyIndex)
						{
							continue;
						}

						auto &queueFamily = queueFamilies[queueFamilyIndex];
						if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
						{
							computeFamilyIndex = queueFamilyIndex;
						}
					}

					// find transfer queue
					for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex)
					{
						// we're trying to find dedicated queues, so skip the graphics/compute queue
						if (queueFamilyIndex == graphicsFamilyIndex || queueFamilyIndex == computeFamilyIndex)
						{
							continue;
						}

						auto &queueFamily = queueFamilies[queueFamilyIndex];
						if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
						{
							transferFamilyIndex = queueFamilyIndex;
						}
					}

					// use graphics queue if no dedicated other queues
					if (computeFamilyIndex == -1)
					{
						computeFamilyIndex = graphicsFamilyIndex;
					}
					if (transferFamilyIndex == -1)
					{
						transferFamilyIndex = graphicsFamilyIndex;
					}

					// query present support
					vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsFamilyIndex, m_surface, &graphicsFamilyPresentable);
					vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, computeFamilyIndex, m_surface, &computeFamilyPresentable);
				}

				// test if all required extensions are supported by this physical device
				bool extensionsSupported;
				{
					uint32_t extensionCount;
					vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

					std::vector<VkExtensionProperties> availableExtensions(extensionCount);
					vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

					std::set<std::string> requiredExtensions(std::begin(deviceExtensions), std::end(deviceExtensions));

					for (const auto& extension : availableExtensions)
					{
						requiredExtensions.erase(extension.extensionName);
					}

					extensionsSupported = requiredExtensions.empty();
				}

				// test if the device supports a swapchain
				bool swapChainAdequate = false;
				if (extensionsSupported)
				{
					VKSwapChainSupportDetails details;

					vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_surface, &details.m_capabilities);

					uint32_t formatCount;
					vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);

					if (formatCount != 0)
					{
						details.m_formats.resize(static_cast<size_t>(formatCount));
						vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, details.m_formats.data());
					}

					uint32_t presentModeCount;
					vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);

					if (presentModeCount != 0)
					{
						details.m_presentModes.resize(static_cast<size_t>(presentModeCount));
						vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, details.m_presentModes.data());
					}

					swapChainSupportDetails = details;
					swapChainAdequate = !swapChainSupportDetails.m_formats.empty() && !swapChainSupportDetails.m_presentModes.empty();
				}

				VkPhysicalDeviceFeatures supportedFeatures;
				vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

				if (graphicsFamilyIndex >= 0
					&& computeFamilyIndex >= 0
					&& transferFamilyIndex >= 0
					&& graphicsFamilyPresentable
					//&& computeFamilyPresentable
					&& extensionsSupported
					&& swapChainAdequate
					&& supportedFeatures.samplerAnisotropy
					&& supportedFeatures.textureCompressionBC
					&& supportedFeatures.independentBlend
					&& supportedFeatures.fragmentStoresAndAtomics
					&& supportedFeatures.shaderStorageImageExtendedFormats
					&& supportedFeatures.multiDrawIndirect)
				{
					m_physicalDevice = physicalDevice;
					m_queueFamilyIndices =
					{
						static_cast<uint32_t>(graphicsFamilyIndex),
						static_cast<uint32_t>(computeFamilyIndex),
						static_cast<uint32_t>(transferFamilyIndex),
						static_cast<bool>(graphicsFamilyPresentable),
						static_cast<bool>(computeFamilyPresentable)
					};
					m_swapChainSupportDetails = swapChainSupportDetails;
					vkGetPhysicalDeviceProperties(physicalDevice, &m_properties);
					m_features = supportedFeatures;
					break;
				}
			}

			if (m_physicalDevice == VK_NULL_HANDLE)
			{
				Utility::fatalExit("Failed to find a suitable GPU!", -1);
			}
		}

		// create logical device and retrieve queues
		{
			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
			std::set<uint32_t> uniqueQueueFamilies{ m_queueFamilyIndices.m_graphicsFamily, m_queueFamilyIndices.m_computeFamily, m_queueFamilyIndices.m_transferFamily };

			float queuePriority = 1.0f;
			for (uint32_t queueFamily : uniqueQueueFamilies)
			{
				VkDeviceQueueCreateInfo queueCreateInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
				queueCreateInfo.queueFamilyIndex = queueFamily;
				queueCreateInfo.queueCount = 1;
				queueCreateInfo.pQueuePriorities = &queuePriority;

				queueCreateInfos.push_back(queueCreateInfo);
			}

			VkPhysicalDeviceFeatures deviceFeatures{};
			deviceFeatures.samplerAnisotropy = VK_TRUE;
			deviceFeatures.textureCompressionBC = VK_TRUE;
			deviceFeatures.independentBlend = VK_TRUE;
			deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
			deviceFeatures.shaderStorageImageExtendedFormats = VK_TRUE;
			deviceFeatures.multiDrawIndirect = VK_TRUE;

			m_enabledFeatures = deviceFeatures;

			VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
			createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
			createInfo.pQueueCreateInfos = queueCreateInfos.data();
			createInfo.enabledLayerCount = 0;
			createInfo.ppEnabledLayerNames = nullptr;
			createInfo.pEnabledFeatures = &deviceFeatures;
			createInfo.enabledExtensionCount = static_cast<uint32_t>(sizeof(deviceExtensions) / sizeof(deviceExtensions[0]));
			createInfo.ppEnabledExtensionNames = deviceExtensions;

			if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create logical device!", -1);
			}

			vkGetDeviceQueue(m_device, m_queueFamilyIndices.m_graphicsFamily, 0, &m_graphicsQueue);
			vkGetDeviceQueue(m_device, m_queueFamilyIndices.m_computeFamily, 0, &m_computeQueue);
			vkGetDeviceQueue(m_device, m_queueFamilyIndices.m_transferFamily, 0, &m_transferQueue);
		}

		// create command pools
		{
			VkCommandPoolCreateInfo graphicsPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			graphicsPoolInfo.queueFamilyIndex = m_queueFamilyIndices.m_graphicsFamily;
			graphicsPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

			if (vkCreateCommandPool(m_device, &graphicsPoolInfo, nullptr, &m_graphicsCommandPool) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create graphics command pool!", -1);
			}

			VkCommandPoolCreateInfo computePoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			computePoolInfo.queueFamilyIndex = m_queueFamilyIndices.m_computeFamily;

			if (vkCreateCommandPool(m_device, &computePoolInfo, nullptr, &m_computeCommandPool) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create compute command pool!", -1);
			}
		}

		// create semaphores
		{
			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore) != VK_SUCCESS
				|| vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphore) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create semaphores!", -1);
			}
		}

		// subgroup properties
		{
			m_subgroupProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };

			VkPhysicalDeviceProperties2 physicalDeviceProperties;
			physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			physicalDeviceProperties.pNext = &m_subgroupProperties;

			vkGetPhysicalDeviceProperties2(m_physicalDevice, &physicalDeviceProperties);
		}

		// create allocator
		m_allocator.init(m_device, m_physicalDevice);
	}

	void VKContext::shutdown()
	{
		vkDestroyDevice(m_device, nullptr);

		if (g_vulkanDebugCallBackEnabled)
		{
			if (auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT"))
			{
				func(m_instance, m_debugCallback, nullptr);
			}
		}

		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyInstance(m_instance, nullptr);
	}
}