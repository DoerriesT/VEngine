#pragma once
#include <vulkan/vulkan.h>
#include <glm/mat4x4.hpp>
#include "Graphics/Vulkan/VKBufferData.h"
#include <vector>
#include <memory>

namespace VEngine
{
	struct VKRenderResources;
	class Model;

	struct UBO
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
	};

	class VKForwardPipeline
	{
	public:
		explicit VKForwardPipeline();
		VKForwardPipeline(const VKForwardPipeline &) = delete;
		VKForwardPipeline(const VKForwardPipeline &&) = delete;
		VKForwardPipeline &operator= (const VKForwardPipeline &) = delete;
		VKForwardPipeline &operator= (const VKForwardPipeline &&) = delete;
		~VKForwardPipeline();
		void init(unsigned int width, unsigned int height, VkRenderPass renderPass, VkBuffer uniformBuffer);
		void recordCommandBuffer(VkRenderPass renderPass, VkFramebuffer framebuffer, VkCommandBuffer commandBuffer, const std::vector<std::shared_ptr<Model>> &models);

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		VkDescriptorPool m_descriptorPool;
		VkDescriptorSet m_descriptorSet;
		VkDescriptorSetLayout m_descriptorSetLayout;
		unsigned int m_width;
		unsigned int m_height;
	};
}
