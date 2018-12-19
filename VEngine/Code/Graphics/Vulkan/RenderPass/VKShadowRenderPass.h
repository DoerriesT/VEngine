#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawLists;
	struct LightData;
	class VKShadowPipeline;

	class VKShadowRenderPass
	{
	public:
		explicit VKShadowRenderPass(VKRenderResources *renderResources);
		VKShadowRenderPass(const VKShadowRenderPass &) = delete;
		VKShadowRenderPass(const VKShadowRenderPass &&) = delete;
		VKShadowRenderPass &operator= (const VKShadowRenderPass &) = delete;
		VKShadowRenderPass &operator= (const VKShadowRenderPass &&) = delete;
		~VKShadowRenderPass();

		VkRenderPass get();
		void record(VKRenderResources *renderResources, const DrawLists &drawLists, const LightData &lightData, uint32_t width, uint32_t height);
		void submit(VKRenderResources *renderResources);
		void setPipelines(VKShadowPipeline *shadowPipeline);
	
	private:
		VkRenderPass m_renderPass;
		VKShadowPipeline *m_shadowPipeline;
	};
}