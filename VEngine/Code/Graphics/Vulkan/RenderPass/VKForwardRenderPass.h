#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawLists;
	class VKForwardPipeline;

	class VKForwardRenderPass
	{
	public:
		explicit VKForwardRenderPass(VKRenderResources *renderResources);
		VKForwardRenderPass(const VKForwardRenderPass &) = delete;
		VKForwardRenderPass(const VKForwardRenderPass &&) = delete;
		VKForwardRenderPass &operator= (const VKForwardRenderPass &) = delete;
		VKForwardRenderPass &operator= (const VKForwardRenderPass &&) = delete;
		~VKForwardRenderPass();

		VkRenderPass get();
		void record(VKRenderResources *renderResources, const DrawLists &drawLists, VkImage swapChainImage, uint32_t width, uint32_t height);
		void submit(VKRenderResources *renderResources);
		void setPipelines(VKForwardPipeline *forwardPipeline);

	private:
		VkRenderPass m_renderPass;
		VKForwardPipeline *m_forwardPipeline; 
	};
}