#pragma once
#include <vulkan/vulkan.h>
#include <cstring>

namespace VEngine
{
	struct VKGraphicsPipelineDescription
	{
		enum
		{
			MAX_PATH_LENGTH = 256,
			MAX_VERTEX_BINDING_DESCRIPTIONS = 8,
			MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS = 8,
			MAX_VIEWPORTS = 1,
			MAX_SCISSORS = 1,
			MAX_COLOR_BLEND_ATTACHMENT_STATES = 8,
			MAX_DYNAMIC_STATES = 9,
			MAX_DESCRIPTOR_SET_LAYOUTS = 8,
			MAX_PUSH_CONSTANT_RANGES = 1,
		};

		struct ShaderStages
		{
			char m_vertexShaderPath[MAX_PATH_LENGTH + 1] = {};
			char m_tesselationControlShaderPass[MAX_PATH_LENGTH + 1] = {};
			char m_tesselationEvaluationShaderPass[MAX_PATH_LENGTH + 1] = {};
			char m_geometryShaderPath[MAX_PATH_LENGTH + 1] = {};
			char m_fragmentShaderPath[MAX_PATH_LENGTH + 1] = {};
		};

		struct VertexInputState
		{
			uint32_t m_vertexBindingDescriptionCount;
			VkVertexInputBindingDescription m_vertexBindingDescriptions[MAX_VERTEX_BINDING_DESCRIPTIONS] = {};
			uint32_t m_vertexAttributeDescriptionCount;
			VkVertexInputAttributeDescription m_vertexAttributeDescriptions[MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS] = {};
		};

		struct InputAssemblyState
		{
			VkPrimitiveTopology m_primitiveTopology;
			bool m_primitiveRestartEnable;
		};

		struct TesselationState
		{
			uint32_t m_patchControlPoints;
		};
		
		struct ViewportState
		{
			uint32_t m_viewportCount;
			VkViewport m_viewports[MAX_VIEWPORTS] = {};
			uint32_t m_scissorCount;
			VkRect2D m_scissors[MAX_SCISSORS] = {};
		};
		
		struct RasterizationState
		{
			bool m_depthClampEnable;
			bool m_rasterizerDiscardEnable;
			VkPolygonMode m_polygonMode;
			VkCullModeFlags m_cullMode;
			VkFrontFace m_frontFace;
			bool m_depthBiasEnable;
			float m_depthBiasConstantFactor;
			float m_depthBiasClamp;
			float m_depthBiasSlopeFactor;
			float m_lineWidth;
		};

		struct MultisampleState
		{
			VkSampleCountFlagBits m_rasterizationSamples;
			bool m_sampleShadingEnable;
			float m_minSampleShading;
			VkSampleMask m_sampleMask;
			bool m_alphaToCoverageEnable;
			bool m_alphaToOneEnable;
		};
		
		struct DepthStencilState
		{
			bool m_depthTestEnable;
			bool m_depthWriteEnable;
			VkCompareOp m_depthCompareOp;
			bool m_depthBoundsTestEnable;
			bool m_stencilTestEnable;
			VkStencilOpState m_front;
			VkStencilOpState m_back;
			float m_minDepthBounds;
			float m_maxDepthBounds;
		};

		struct BlendState
		{
			bool m_logicOpEnable;
			VkLogicOp m_logicOp;
			uint32_t m_attachmentCount;
			VkPipelineColorBlendAttachmentState m_attachments[MAX_COLOR_BLEND_ATTACHMENT_STATES] = {};
			float m_blendConstants[4];
		};

		struct DynamicState
		{
			uint32_t m_dynamicStateCount;
			VkDynamicState m_dynamicStates[MAX_DYNAMIC_STATES] = {};
		};

		struct Layout
		{
			uint32_t m_setLayoutCount;
			VkDescriptorSetLayout m_setLayouts[MAX_DESCRIPTOR_SET_LAYOUTS] = {};
			uint32_t m_pushConstantRangeCount;
			VkPushConstantRange m_pushConstantRanges[MAX_PUSH_CONSTANT_RANGES] = {};
		};

		struct RenderPassDescription
		{
			enum
			{
				MAX_ATTACHMENTS = 9,
				MAX_INPUT_ATTACHMENTS = 8,
				MAX_COLOR_ATTACHMENTS = 8,
				MAX_RESOLVE_ATTACHMENTS = 8,
			};

			struct AttachmentDescription
			{
				VkFormat m_format;
				VkSampleCountFlagBits m_samples;
			};

			uint32_t m_attachmentCount;
			AttachmentDescription m_attachments[MAX_ATTACHMENTS] = {};
			uint32_t m_inputAttachmentCount;
			VkAttachmentReference m_inputAttachments[MAX_INPUT_ATTACHMENTS] = {};
			uint32_t m_colorAttachmentCount;
			VkAttachmentReference m_colorAttachments[MAX_COLOR_ATTACHMENTS] = {};
			uint32_t m_resolveAttachmentCount;
			VkAttachmentReference m_resolveAttachments[MAX_RESOLVE_ATTACHMENTS] = {};
			bool m_depthStencilAttachmentPresent;
			VkAttachmentReference m_depthStencilAttachment;
		};

		ShaderStages m_shaderStages;
		VertexInputState m_vertexInputState;
		InputAssemblyState m_inputAssemblyState;
		TesselationState m_tesselationState;
		ViewportState m_viewportState;
		RasterizationState m_rasterizationState;
		MultisampleState m_multiSampleState;
		DepthStencilState m_depthStencilState;
		BlendState m_blendState;
		DynamicState m_dynamicState;
		Layout m_layout;
		RenderPassDescription m_renderpass;

		VKGraphicsPipelineDescription()
		{
			memset(this, 0, sizeof(*this));
		}
	};

	struct VKComputePipelineDescription
	{
		enum
		{
			MAX_PATH_LENGTH = 256,
			MAX_DESCRIPTOR_SET_LAYOUTS = 8,
			MAX_PUSH_CONSTANT_RANGES = 1,
		};

		struct Layout
		{
			uint32_t m_setLayoutCount;
			VkDescriptorSetLayout m_setLayouts[MAX_DESCRIPTOR_SET_LAYOUTS] = {};
			uint32_t m_pushConstantRangeCount;
			VkPushConstantRange m_pushConstantRanges[MAX_PUSH_CONSTANT_RANGES] = {};
		};

		char m_computeShaderPath[MAX_PATH_LENGTH + 1] = {};
		Layout m_layout;

		VKComputePipelineDescription()
		{
			memset(this, 0, sizeof(*this));
		}
	};

	inline bool operator==(const VKGraphicsPipelineDescription &lhs, const VKGraphicsPipelineDescription &rhs)
	{
		return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
	}

	inline bool operator==(const VKComputePipelineDescription &lhs, const VKComputePipelineDescription &rhs)
	{
		return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
	}

	struct VKGraphicsPipelineDescriptionHash
	{
		size_t operator()(const VKGraphicsPipelineDescription &value) const;
	};

	struct VKComputePipelineDescriptionHash
	{
		size_t operator()(const VKComputePipelineDescription &value) const;
	};

}