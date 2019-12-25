#pragma once
#include "VKPipeline.h"
#include <unordered_map>

namespace VEngine
{
	struct DescriptorSetLayoutData
	{
		enum
		{
			MAX_SET_COUNT = 8
		};

		uint32_t m_counts[MAX_SET_COUNT][VK_DESCRIPTOR_TYPE_RANGE_SIZE] = {};
		VkDescriptorSetLayout m_layouts[MAX_SET_COUNT];
		uint32_t m_layoutCount;
	};

	class VKPipelineCache
	{
	public:
		struct PipelineData
		{
			DescriptorSetLayoutData m_descriptorSetLayoutData;
			VkPipeline m_pipeline;
			VkPipelineLayout m_layout;
		};

		PipelineData getPipeline(const GraphicsPipelineDesc &pipelineDesc, const RenderPassCompatDesc &renderPassDesc, uint32_t subpassIndex, VkRenderPass renderPass);
		PipelineData getPipeline(const ComputePipelineDesc &pipelineDesc);

	private:
		struct ReflectionInfo
		{
			enum
			{
				MAX_SET_COUNT = 8,
				MAX_BINDING_COUNT = 32,
			};

			struct SetLayout
			{
				uint32_t m_uniformBufferMask;
				uint32_t m_uniformTexelBufferMask;
				uint32_t m_storageBufferMask;
				uint32_t m_storageTexelBufferMask;
				uint32_t m_subpassInputMask;
				uint32_t m_storageImageMask;
				uint32_t m_sampledImageMask;
				uint32_t m_separateImageMask;
				uint32_t m_separateSamplerMask;
				uint32_t m_arraySizes[MAX_BINDING_COUNT];
				VkShaderStageFlags m_stageFlags[MAX_BINDING_COUNT];
			};

			struct PushConstantInfo
			{
				uint32_t m_size;
				VkShaderStageFlags m_stageFlags;
			};

			SetLayout m_setLayouts[MAX_SET_COUNT];
			PushConstantInfo m_pushConstants;
			uint32_t m_setMask;
		};

		std::unordered_map<CombinedGraphicsPipelineRenderPassDesc, PipelineData, CombinedGraphicsPipelineRenderPassDescHash> m_graphicsPipelines;
		std::unordered_map<ComputePipelineDesc, PipelineData, ComputePipelineDescHash> m_computePipelines;

		void createShaderStage(const ShaderStageDesc &stageDescription, VkShaderStageFlagBits stageFlag, VkShaderModule &shaderModule, VkPipelineShaderStageCreateInfo &stageCreateInfo, ReflectionInfo &reflectionInfo, VkSpecializationInfo & specInfo);
		void createPipelineLayout(const ReflectionInfo &reflectionInfo, VkPipelineLayout &pipelineLayout, DescriptorSetLayoutData &descriptorSetLayoutData);
	};
}
