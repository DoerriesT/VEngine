#pragma once
#include "Graphics/Vulkan/volk.h"

namespace VEngine
{
	namespace gal
	{
		namespace UtilityVk
		{
			VkResult checkResult(VkResult result, const char *errorMsg = nullptr, bool exitOnError = true);
			VkImageAspectFlags getImageAspectMask(VkFormat format);
		}
	}
}