#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;

	struct VKMainRenderPass
	{
		VkRenderPass m_renderPass;

		explicit VKMainRenderPass(VKRenderResources *renderResources);
		VKMainRenderPass(const VKMainRenderPass &) = delete;
		VKMainRenderPass(const VKMainRenderPass &&) = delete;
		VKMainRenderPass &operator= (const VKMainRenderPass &) = delete;
		VKMainRenderPass &operator= (const VKMainRenderPass &&) = delete;
		~VKMainRenderPass();
	};
}