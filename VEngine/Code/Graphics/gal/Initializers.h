#pragma once
#include "GraphicsAbstractionLayer.h"

namespace VEngine
{
	namespace gal
	{
		namespace Initializers
		{
			DescriptorSetUpdate samplerDescriptor(const Sampler *const *samplers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
			DescriptorSetUpdate sampledImage(const ImageView *const *images, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
			DescriptorSetUpdate storageImage(const ImageView *const *images, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
			DescriptorSetUpdate uniformTexelBuffer(const BufferView *const *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
			DescriptorSetUpdate storageTexelBuffer(const BufferView *const *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
			DescriptorSetUpdate uniformBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
			DescriptorSetUpdate storageBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement = 0, uint32_t count = 1);
			Barrier imageBarrier(const Image *image, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter, const ImageSubresourceRange &subresourceRange = { 0, 1, 0, 1 });
			Barrier bufferBarrier(const Buffer *buffer, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter);
			void submitSingleTimeCommands(Queue *queue, CommandList *cmdList);
			bool isReadAccess(ResourceState state);
			bool isWriteAccess(ResourceState state);
			uint32_t getUsageFlags(ResourceState state);
			bool isDepthFormat(Format format);
		}

		class GraphicsPipelineBuilder
		{
		public:
			static const PipelineColorBlendAttachmentState s_defaultBlendAttachment;

			explicit GraphicsPipelineBuilder(GraphicsPipelineCreateInfo &createInfo);
			void setVertexShader(const char *path);
			void setTessControlShader(const char *path);
			void setTessEvalShader(const char *path);
			void setGeometryShader(const char *path);
			void setFragmentShader(const char *path);
			void setVertexBindingDescriptions(size_t count, const VertexInputBindingDescription *bindingDescs);
			void setVertexBindingDescription(const VertexInputBindingDescription &bindingDesc);
			void setVertexAttributeDescriptions(size_t count, const VertexInputAttributeDescription *attributeDescs);
			void setVertexAttributeDescription(const VertexInputAttributeDescription &attributeDesc);
			void setInputAssemblyState(PrimitiveTopology topology, bool primitiveRestartEnable);
			void setTesselationState(uint32_t patchControlPoints);
			void setViewportScissors(size_t count, const Viewport *viewports, const Rect *scissors);
			void setViewportScissor(const Viewport &viewport, const Rect &scissor);
			void setDepthClampEnable(bool depthClampEnable);
			void setRasterizerDiscardEnable(bool rasterizerDiscardEnable);
			void setPolygonModeCullMode(PolygonMode polygonMode, CullModeFlags cullMode, FrontFace frontFace);
			void setDepthBias(bool enable, float constantFactor, float clamp, float slopeFactor);
			void setLineWidth(float lineWidth);
			void setMultisampleState(SampleCount rasterizationSamples, bool sampleShadingEnable, float minSampleShading, uint32_t sampleMask, bool alphaToCoverageEnable, bool alphaToOneEnable);
			void setDepthTest(bool depthTestEnable, bool depthWriteEnable, CompareOp depthCompareOp);
			void setStencilTest(bool stencilTestEnable, const StencilOpState &front, const StencilOpState &back);
			void setDepthBoundsTest(bool depthBoundsTestEnable, float minDepthBounds, float maxDepthBounds);
			void setBlendStateLogicOp(bool logicOpEnable, LogicOp logicOp);
			void setBlendConstants(float blendConst0, float blendConst1, float blendConst2, float blendConst3);
			void setColorBlendAttachments(size_t count, const PipelineColorBlendAttachmentState *colorBlendAttachments);
			void setColorBlendAttachment(const PipelineColorBlendAttachmentState &colorBlendAttachment);
			void setDynamicState(size_t count, const DynamicState *dynamicState);
			void setDynamicState(DynamicState dynamicState);
			void setColorAttachmentFormats(uint32_t count, Format *formats);
			void setColorAttachmentFormat(Format format);
			void setDepthStencilAttachmentFormat(Format format);

		private:
			GraphicsPipelineCreateInfo &m_createInfo;
		};

		class ComputePipelineBuilder
		{
		public:
			explicit ComputePipelineBuilder(ComputePipelineCreateInfo &createInfo);
			void setComputeShader(const char *path);

		private:
			ComputePipelineCreateInfo &m_createInfo;
		};
	}
}