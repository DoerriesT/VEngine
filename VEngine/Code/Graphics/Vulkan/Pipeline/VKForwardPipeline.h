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
		void init(unsigned int width, unsigned int height, VKRenderResources *renderResources);
		void recordCommandBuffer(const std::vector<std::shared_ptr<Model>> &models);
		void execute();
		VkCommandBuffer getCommandBuffer() const;
		VKBufferData getUniformBuffer() const;

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		VkRenderPass m_renderPass;
		VkDescriptorPool m_descriptorPool;
		VkDescriptorSet m_descriptorSet;
		VkDescriptorSetLayout m_descriptorSetLayout;
		VkFramebuffer m_framebuffer;
		VkCommandBuffer m_commandBuffer;
		VKBufferData m_uniformBuffer;
		unsigned int m_width;
		unsigned int m_height;
	};
}
