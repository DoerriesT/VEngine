#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "Graphics/Vulkan/volk.h"
#include "DescriptorSetVk.h"
#include "Utility/ObjectPool.h"

namespace VEngine
{
	namespace gal
	{
		struct GraphicsPipelineCreateInfo;
		struct ComputePipelineCreateInfo;
		class GraphicsDeviceVk;

		struct DescriptorSetLayoutsVk
		{
			enum { MAX_SET_COUNT = 8 };
			StaticObjectPool<ByteArray<sizeof(DescriptorSetLayoutVk)>, MAX_SET_COUNT> m_descriptorSetLayoutMemoryPool;
			DescriptorSetLayoutVk *m_descriptorSetLayouts[MAX_SET_COUNT];
			uint32_t m_layoutCount;
		};

		struct DescriptorSetLayoutDataVk
		{
			uint32_t m_counts[DescriptorSetLayoutsVk::MAX_SET_COUNT][VK_DESCRIPTOR_TYPE_RANGE_SIZE] = {};
		};

		class GraphicsPipelineVk : public GraphicsPipeline
		{
		public:
			explicit GraphicsPipelineVk(GraphicsDeviceVk &device, const GraphicsPipelineCreateInfo &createInfo);
			GraphicsPipelineVk(GraphicsPipelineVk &) = delete;
			GraphicsPipelineVk(GraphicsPipelineVk &&) = delete;
			GraphicsPipelineVk &operator= (const GraphicsPipelineVk &) = delete;
			GraphicsPipelineVk &operator= (const GraphicsPipelineVk &&) = delete;
			~GraphicsPipelineVk();
			void *getNativeHandle() const override;
			uint32_t getDescriptorSetLayoutCount() const;
			const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const;
			VkPipelineLayout getLayout() const;
		private:
			VkPipeline m_pipeline;
			VkPipelineLayout m_pipelineLayout;
			GraphicsDeviceVk *m_device;
			DescriptorSetLayoutsVk m_descriptorSetLayouts;
		};

		class ComputePipelineVk : public ComputePipeline
		{
		public:
			explicit ComputePipelineVk(GraphicsDeviceVk &device, const ComputePipelineCreateInfo &createInfo);
			ComputePipelineVk(ComputePipelineVk &) = delete;
			ComputePipelineVk(ComputePipelineVk &&) = delete;
			ComputePipelineVk &operator= (const ComputePipelineVk &) = delete;
			ComputePipelineVk &operator= (const ComputePipelineVk &&) = delete;
			~ComputePipelineVk();
			void *getNativeHandle() const override;
			uint32_t getDescriptorSetLayoutCount() const;
			const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const;
			VkPipelineLayout getLayout() const;
		private:
			enum { MAX_SET_COUNT };
			VkPipeline m_pipeline;
			VkPipelineLayout m_pipelineLayout;
			GraphicsDeviceVk *m_device;
			DescriptorSetLayoutsVk m_descriptorSetLayouts;
		};
	}
}