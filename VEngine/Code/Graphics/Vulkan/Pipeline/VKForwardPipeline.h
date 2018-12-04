#pragma once
#include <vulkan/vulkan.h>
#include <glm/mat4x4.hpp>
#include <vector>
#include <memory>

namespace VEngine
{
	struct VKRenderResources;
	struct DrawItem;

	class VKForwardPipeline
	{
	public:
		explicit VKForwardPipeline();
		VKForwardPipeline(const VKForwardPipeline &) = delete;
		VKForwardPipeline(const VKForwardPipeline &&) = delete;
		VKForwardPipeline &operator= (const VKForwardPipeline &) = delete;
		VKForwardPipeline &operator= (const VKForwardPipeline &&) = delete;
		~VKForwardPipeline();
		void init(unsigned int width, unsigned int height, VkRenderPass renderPass, VKRenderResources *renderResourcese);
		void recordCommandBuffer(VkRenderPass renderPass, VKRenderResources *renderResources, const std::vector<DrawItem> &drawItems);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
