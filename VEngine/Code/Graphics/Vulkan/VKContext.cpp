#include "VKContext.h"
#include "Utility/Utility.h"
#include <iostream>
#include <GLFW/glfw3.h>
#include <set>

VEngine::VKContext g_context = {};

namespace VEngine
{
	static const char* const validationLayers[] = { "VK_LAYER_LUNARG_standard_validation" };
	static const char* const deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
	{
		std::cerr << "validation layer: " << msg << std::endl;

		return VK_FALSE;
	}

	void VKContext::init(GLFWwindow * windowHandle)
	{
#if ENABLE_VALIDATION_LAYERS
		// validation layers
		{
			uint32_t layerCount;
			vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

			std::vector<VkLayerProperties> availableLayers(layerCount);
			vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

			for (const char* layerName : validationLayers)
			{
				bool layerFound = false;

				for (const auto& layerProperties : availableLayers)
				{
					if (strcmp(layerName, layerProperties.layerName) == 0)
					{
						layerFound = true;
						break;
					}
				}

				if (!layerFound)
				{
					Utility::fatalExit("Validation layers requested, but not available!", -1);
				}
			}
		}
#endif // ENABLE_VALIDATION_LAYERS

		// create instance
		{
			VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
			appInfo.pApplicationName = "Vulkan";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "No Engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_0;

			// extensions
			uint32_t glfwExtensionCount = 0;
			const char** glfwExtensions;
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
			std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#if ENABLE_VALIDATION_LAYERS
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif // ENABLE_VALIDATION_LAYERS


			VkInstanceCreateInfo createInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
			createInfo.pApplicationInfo = &appInfo;
			createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
			createInfo.ppEnabledExtensionNames = extensions.data();

#if ENABLE_VALIDATION_LAYERS
			createInfo.enabledLayerCount = static_cast<uint32_t>(sizeof(validationLayers) / sizeof(validationLayers[0]));
			createInfo.ppEnabledLayerNames = validationLayers;
#else
			createInfo.enabledLayerCount = 0;
#endif // ENABLE_VALIDATION_LAYERS

			if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create instance!", -1);
			}
		}

#if ENABLE_VALIDATION_LAYERS
		// create debug callback
		{
			VkDebugReportCallbackCreateInfoEXT createInfo = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
			createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
			createInfo.pfnCallback = debugCallback;

			auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");
			if (!func || func(m_instance, &createInfo, nullptr, &m_debugCallback) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to set up debug callback!", -1);
			}
		}
#endif // ENABLE_VALIDATION_LAYERS

		// create window surface
		{
			if (glfwCreateWindowSurface(m_instance, windowHandle, nullptr, &m_surface) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create window surface!", -1);
			}
		}

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
				int presentFamilyIndex = -1;
				{
					uint32_t queueFamilyCount = 0;
					vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

					std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
					vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

					for (uint32_t i = 0; i < queueFamilyCount; ++i)
					{
						auto &queueFamily = queueFamilies[i];
						if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
						{
							graphicsFamilyIndex = i;
						}

						if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
						{
							computeFamilyIndex = i;
						}

						if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
						{
							transferFamilyIndex = i;
						}

						VkBool32 presentSupport = false;
						vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_surface, &presentSupport);

						if (queueFamily.queueCount > 0 && presentSupport)
						{
							presentFamilyIndex = i;
						}

						if (graphicsFamilyIndex >= 0 && presentFamilyIndex >= 0 && computeFamilyIndex >= 0 && transferFamilyIndex >= 0)
						{
							break;
						}
					}
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
						details.m_formats.resize(formatCount);
						vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, details.m_formats.data());
					}

					uint32_t presentModeCount;
					vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);

					if (presentModeCount != 0)
					{
						details.m_presentModes.resize(presentModeCount);
						vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, details.m_presentModes.data());
					}

					swapChainSupportDetails = details;
					swapChainAdequate = !swapChainSupportDetails.m_formats.empty() && !swapChainSupportDetails.m_presentModes.empty();
				}

				VkPhysicalDeviceFeatures supportedFeatures;
				vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

				if (graphicsFamilyIndex >= 0
					&& presentFamilyIndex >= 0
					&& computeFamilyIndex >= 0
					&& transferFamilyIndex >= 0
					&& extensionsSupported
					&& swapChainAdequate
					&& supportedFeatures.samplerAnisotropy)
				{
					m_physicalDevice = physicalDevice;
					m_queueFamilyIndices = 
					{ 
						static_cast<uint32_t>(graphicsFamilyIndex),  
						static_cast<uint32_t>(presentFamilyIndex),  
						static_cast<uint32_t>(computeFamilyIndex ),  
						static_cast<uint32_t>(transferFamilyIndex )
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
			std::set<uint32_t> uniqueQueueFamilies = { m_queueFamilyIndices.m_graphicsFamily, m_queueFamilyIndices.m_presentFamily, m_queueFamilyIndices.m_computeFamily, m_queueFamilyIndices.m_transferFamily };

			float queuePriority = 1.0f;
			for (uint32_t queueFamily : uniqueQueueFamilies)
			{
				VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
				queueCreateInfo.queueFamilyIndex = queueFamily;
				queueCreateInfo.queueCount = 1;
				queueCreateInfo.pQueuePriorities = &queuePriority;

				queueCreateInfos.push_back(queueCreateInfo);
			}

			VkPhysicalDeviceFeatures deviceFeatures = {};
			deviceFeatures.samplerAnisotropy = VK_TRUE;

			VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
			createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
			createInfo.pQueueCreateInfos = queueCreateInfos.data();
			createInfo.pEnabledFeatures = &deviceFeatures;
			createInfo.enabledExtensionCount = static_cast<uint32_t>(sizeof(deviceExtensions) / sizeof(deviceExtensions[0]));
			createInfo.ppEnabledExtensionNames = deviceExtensions;

#if ENABLE_VALIDATION_LAYERS
			createInfo.enabledLayerCount = static_cast<uint32_t>(sizeof(validationLayers) / sizeof(validationLayers[0]));
			createInfo.ppEnabledLayerNames = validationLayers;
#else
			createInfo.enabledLayerCount = 0;
#endif // ENABLE_VALIDATION_LAYERS

			if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create logical device!", -1);
			}

			vkGetDeviceQueue(m_device, m_queueFamilyIndices.m_graphicsFamily, 0, &m_graphicsQueue);
			vkGetDeviceQueue(m_device, m_queueFamilyIndices.m_presentFamily, 0, &m_presentQueue);
			vkGetDeviceQueue(m_device, m_queueFamilyIndices.m_presentFamily, 0, &m_computeQueue);
			vkGetDeviceQueue(m_device, m_queueFamilyIndices.m_transferFamily, 0, &m_transferQueue);
		}

		// create command pools
		{
			VkCommandPoolCreateInfo graphicsPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			graphicsPoolInfo.queueFamilyIndex = m_queueFamilyIndices.m_graphicsFamily;

			if (vkCreateCommandPool(m_device, &graphicsPoolInfo, nullptr, &m_graphicsCommandPool) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create graphics command pool!", -1);
			}

			VkCommandPoolCreateInfo computePoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			computePoolInfo.queueFamilyIndex = m_queueFamilyIndices.m_computeFamily;

			if (vkCreateCommandPool(m_device, &computePoolInfo, nullptr, &m_computeCommandPool) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create compute command pool!", -1);
			}
		}

		// create semaphores
		{
			VkSemaphoreCreateInfo semaphoreInfo = {};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore) != VK_SUCCESS
				|| vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphore) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create semaphores!", -1);
			}
		}
	}

	void VKContext::shutdown()
	{
		vkDestroyDevice(m_device, nullptr);

#if ENABLE_VALIDATION_LAYERS
			if (auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT"))
			{
				func(m_instance, m_debugCallback, nullptr);
			}
#endif // ENABLE_VALIDATION_LAYERS

		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyInstance(m_instance, nullptr);
	}
}