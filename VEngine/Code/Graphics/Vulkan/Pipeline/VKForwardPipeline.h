#pragma once
#include <vulkan/vulkan.h>
#include <glm/mat4x4.hpp>
#include "Graphics/Vulkan/VKBufferData.h"
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
		void init(unsigned int width, unsigned int height, VkRenderPass renderPass, VkBuffer uniformBuffer,
			VkDeviceSize perFrameDataOffset, VkDeviceSize perFrameDataSize,
			VkDeviceSize perDrawDataOffset, VkDeviceSize perDrawDataSize);
		void recordCommandBuffer(VkRenderPass renderPass, VkFramebuffer framebuffer, VkCommandBuffer commandBuffer, VKBufferData vertexBuffer, VKBufferData indexBuffer, uint32_t uniformBufferIncrement, const std::vector<DrawItem> &drawItems);

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
