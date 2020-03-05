#pragma once
#include "Common.h"

namespace VEngine
{
	namespace gal
	{
		namespace Initializers
		{
			Barrier imageBarrier(const Image *image, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter);
			Barrier bufferBarrier(const Buffer *buffer, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter);
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
	}
}