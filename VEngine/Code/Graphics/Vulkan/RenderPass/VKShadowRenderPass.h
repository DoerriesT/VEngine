#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;

	struct VKShadowRenderPass
	{
		VkRenderPass m_renderPass;

		explicit VKShadowRenderPass(VKRenderResources *renderResources);
		VKShadowRenderPass(const VKShadowRenderPass &) = delete;
		VKShadowRenderPass(const VKShadowRenderPass &&) = delete;
		VKShadowRenderPass &operator= (const VKShadowRenderPass &) = delete;
		VKShadowRenderPass &operator= (const VKShadowRenderPass &&) = delete;
		~VKShadowRenderPass();
	};
}