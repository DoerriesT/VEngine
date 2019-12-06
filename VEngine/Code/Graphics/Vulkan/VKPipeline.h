#pragma once
#include "Graphics/Vulkan/volk.h"

namespace VEngine
{
	struct SpecEntry
	{
		union Value
		{
			int32_t m_iv;
			uint32_t m_uv;
			float m_fv;
		};

		uint32_t m_constantID;
		Value m_value;

		explicit SpecEntry(uint32_t id, int32_t iv) : m_constantID(id), m_value{} { m_value.m_iv = iv; };
		explicit SpecEntry(uint32_t id, uint32_t uv) : m_constantID(id), m_value{} { m_value.m_uv = uv; };
		explicit SpecEntry(uint32_t id, float fv) :m_constantID(id), m_value{} { m_value.m_fv = fv; };
	};

	struct ShaderStageDesc
	{
		enum
		{
			MAX_PATH_LENGTH = 255,
			MAX_SPECIALIZATION_ENTRY_COUNT = 32
		};
		char m_path[MAX_PATH_LENGTH + 1] = {};
		uint8_t m_specializationData[MAX_SPECIALIZATION_ENTRY_COUNT * 4] = {};
		VkSpecializationMapEntry m_specializationEntries[MAX_SPECIALIZATION_ENTRY_COUNT] = {};
		uint32_t m_specConstCount = 0;

		explicit ShaderStageDesc();
	};

	struct GraphicsPipelineDesc
	{
		friend class VKPipelineCache;
		friend struct GraphicsPipelineDescHash;
		friend struct CombinedGraphicsPipelineRenderPassDescHash;
	public:
		static const VkPipelineColorBlendAttachmentState s_defaultBlendAttachment;

		explicit GraphicsPipelineDesc();
		void setVertexShader(const char *path, size_t specializationConstCount = 0, const SpecEntry *specializationConsts = nullptr);
		void setTessControlShader(const char *path, size_t specializationConstCount = 0, const SpecEntry *specializationConsts = nullptr);
		void setTessEvalShader(const char *path, size_t specializationConstCount = 0, const SpecEntry *specializationConsts = nullptr);
		void setGeometryShader(const char *path, size_t specializationConstCount = 0, const SpecEntry *specializationConsts = nullptr);
		void setFragmentShader(const char *path, size_t specializationConstCount = 0, const SpecEntry *specializationConsts = nullptr);
		void setVertexBindingDescriptions(size_t count, const VkVertexInputBindingDescription *bindingDescs);
		void setVertexBindingDescription(const VkVertexInputBindingDescription &bindingDesc);
		void setVertexAttributeDescriptions(size_t count, const VkVertexInputAttributeDescription *attributeDescs);
		void setVertexAttributeDescription(const VkVertexInputAttributeDescription &attributeDesc);
		void setInputAssemblyState(VkPrimitiveTopology topology, bool primitiveRestartEnable);
		void setTesselationState(uint32_t patchControlPoints);
		void setViewportScissors(size_t count, const VkViewport *viewports, const VkRect2D *scissors);
		void setViewportScissor(const VkViewport &viewport, const VkRect2D &scissor);
		void setDepthClampEnable(bool depthClampEnable);
		void setRasterizerDiscardEnable(bool rasterizerDiscardEnable);
		void setPolygonModeCullMode(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace);
		void setDepthBias(bool enable, float constantFactor, float clamp, float slopeFactor);
		void setLineWidth(float lineWidth);
		void setMultisampleState(VkSampleCountFlagBits rasterizationSamples, bool sampleShadingEnable, float minSampleShading, VkSampleMask sampleMask, bool alphaToCoverageEnable, bool alphaToOneEnable);
		void setDepthTest(bool depthTestEnable, bool depthWriteEnable, VkCompareOp depthCompareOp);
		void setStencilTest(bool stencilTestEnable, const VkStencilOpState &front, const VkStencilOpState &back);
		void setDepthBoundsTest(bool depthBoundsTestEnable, float minDepthBounds, float maxDepthBounds);
		void setBlendStateLogicOp(bool logicOpEnable, VkLogicOp logicOp);
		void setBlendConstants(float blendConst0, float blendConst1, float blendConst2, float blendConst3);
		void setColorBlendAttachments(size_t count, const VkPipelineColorBlendAttachmentState *colorBlendAttachments);
		void setColorBlendAttachment(const VkPipelineColorBlendAttachmentState &colorBlendAttachment);
		void setDynamicState(size_t count, const VkDynamicState *dynamicState);
		void setDynamicState(VkDynamicState dynamicState);
		void finalize();

	private:
		enum
		{
			MAX_VERTEX_BINDING_DESCRIPTIONS = 8,
			MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS = 8,
			MAX_VIEWPORTS = 1,
			MAX_COLOR_BLEND_ATTACHMENT_STATES = 8,
			MAX_DYNAMIC_STATES = 9,
		};

		struct VertexInputState
		{
			uint32_t m_vertexBindingDescriptionCount = 0;
			VkVertexInputBindingDescription m_vertexBindingDescriptions[MAX_VERTEX_BINDING_DESCRIPTIONS] = {};
			uint32_t m_vertexAttributeDescriptionCount = 0;
			VkVertexInputAttributeDescription m_vertexAttributeDescriptions[MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS] = {};
		};

		struct InputAssemblyState
		{
			VkPrimitiveTopology m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			uint32_t m_primitiveRestartEnable = false;
		};

		struct TesselationState
		{
			uint32_t m_patchControlPoints = 0;
		};

		struct ViewportState
		{
			uint32_t m_viewportCount = 1;
			VkViewport m_viewports[MAX_VIEWPORTS] = { { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f } };
			VkRect2D m_scissors[MAX_VIEWPORTS] = { { {0, 0}, {1, 1} } };
		};

		struct RasterizationState
		{
			uint32_t m_depthClampEnable = false;
			uint32_t m_rasterizerDiscardEnable = false;
			VkPolygonMode m_polygonMode = VK_POLYGON_MODE_FILL;
			VkCullModeFlags m_cullMode = VK_CULL_MODE_NONE;
			VkFrontFace m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			uint32_t m_depthBiasEnable = false;
			float m_depthBiasConstantFactor = 1.0f;
			float m_depthBiasClamp = 0.0f;
			float m_depthBiasSlopeFactor = 1.0f;
			float m_lineWidth = 1.0f;
		};

		struct MultisampleState
		{
			VkSampleCountFlagBits m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			uint32_t m_sampleShadingEnable = false;
			float m_minSampleShading = 0.0f;
			VkSampleMask m_sampleMask = 0xFFFFFFFF;
			uint32_t m_alphaToCoverageEnable = false;
			uint32_t m_alphaToOneEnable = false;
		};

		struct DepthStencilState
		{
			uint32_t m_depthTestEnable = false;
			uint32_t m_depthWriteEnable = false;
			VkCompareOp m_depthCompareOp = VK_COMPARE_OP_ALWAYS;
			uint32_t m_depthBoundsTestEnable = false;
			uint32_t m_stencilTestEnable = false;
			VkStencilOpState m_front = {};
			VkStencilOpState m_back = {};
			float m_minDepthBounds = 0.0f;
			float m_maxDepthBounds = 1.0f;
		};

		struct BlendState
		{
			uint32_t m_logicOpEnable = false;
			VkLogicOp m_logicOp = VK_LOGIC_OP_COPY;
			uint32_t m_attachmentCount = 0;
			VkPipelineColorBlendAttachmentState m_attachments[MAX_COLOR_BLEND_ATTACHMENT_STATES] = {};
			float m_blendConstants[4] = {};
		};

		struct DynamicState
		{
			uint32_t m_dynamicStateCount = 0;
			VkDynamicState m_dynamicStates[MAX_DYNAMIC_STATES] = {};
		};

		ShaderStageDesc m_vertexShader;
		ShaderStageDesc m_tessControlShader;
		ShaderStageDesc m_tessEvalShader;
		ShaderStageDesc m_geometryShader;
		ShaderStageDesc m_fragmentShader;

		VertexInputState m_vertexInputState;
		InputAssemblyState m_inputAssemblyState;
		TesselationState m_tesselationState;
		ViewportState m_viewportState;
		RasterizationState m_rasterizationState;
		MultisampleState m_multiSampleState;
		DepthStencilState m_depthStencilState;
		BlendState m_blendState;
		DynamicState m_dynamicState;
		size_t m_hashValue;

		void setShader(ShaderStageDesc &shader, const char *path, size_t specializationConstCount = 0, const SpecEntry *specializationConsts = nullptr);
	};

	struct ComputePipelineDesc
	{
		friend class VKPipelineCache;
		friend struct ComputePipelineDescHash;
	public:
		explicit ComputePipelineDesc();
		void setComputeShader(const char *path, size_t specializationConstCount = 0, const SpecEntry *specializationConsts = nullptr);
		void finalize();

	private:
		ShaderStageDesc m_computeShader;
		size_t m_hashValue;
	};

	struct RenderPassDesc
	{
		enum
		{
			MAX_INPUT_ATTACHMENTS = 8,
			MAX_COLOR_ATTACHMENTS = 8,
			MAX_ATTACHMENTS = MAX_INPUT_ATTACHMENTS + MAX_COLOR_ATTACHMENTS + 1, // +1 is for depth attachment
			MAX_SUBPASSES = 4,
			MAX_DEPENDENCIES = MAX_SUBPASSES * MAX_SUBPASSES, // should be enough for each subpass to have a dependency to all previous subpasses
		};

		struct SubpassDesc
		{
			uint8_t m_inputAttachmentCount = 0;
			uint8_t m_colorAttachmentCount = 0;
			bool m_depthStencilAttachmentPresent = false;
			uint8_t m_preserveAttachmentCount = 0;
			VkAttachmentReference m_inputAttachments[MAX_INPUT_ATTACHMENTS] = {};
			VkAttachmentReference m_colorAttachments[MAX_COLOR_ATTACHMENTS] = {};
			VkAttachmentReference m_resolveAttachments[MAX_COLOR_ATTACHMENTS] = {};
			VkAttachmentReference m_depthStencilAttachment = {};
			uint32_t m_preserveAttachments[MAX_ATTACHMENTS];
		};

		uint8_t m_attachmentCount = 0;
		uint8_t m_subpassCount = 1;
		uint8_t m_dependencyCount = 0;
		VkAttachmentDescription m_attachments[MAX_ATTACHMENTS] = {};
		SubpassDesc m_subpasses[MAX_SUBPASSES] = {};
		VkSubpassDependency m_dependencies[MAX_DEPENDENCIES] = {};
		size_t m_hashValue;

		explicit RenderPassDesc();
		// cleans up array elements past array count and precomputes a hash value
		void finalize();
	};

	struct RenderPassCompatDesc
	{
		struct SubpassDesc
		{
			uint8_t m_inputAttachmentCount = 0;
			uint8_t m_colorAttachmentCount = 0;
			bool m_depthStencilAttachmentPresent = false;
			uint8_t m_preserveAttachmentCount = 0;
			uint8_t m_inputAttachments[RenderPassDesc::MAX_INPUT_ATTACHMENTS] = {};
			uint8_t m_colorAttachments[RenderPassDesc::MAX_COLOR_ATTACHMENTS] = {};
			uint8_t m_resolveAttachments[RenderPassDesc::MAX_COLOR_ATTACHMENTS] = {};
			uint8_t m_depthStencilAttachment = {};
			uint32_t m_preserveAttachments[RenderPassDesc::MAX_ATTACHMENTS];
		};

		struct AttachmentDesc
		{
			VkFormat m_format;
			VkSampleCountFlagBits m_samples;
		};

		uint8_t m_attachmentCount = 0;
		uint8_t m_subpassCount = 0;
		uint8_t m_dependencyCount = 0;
		AttachmentDesc m_attachments[RenderPassDesc::MAX_ATTACHMENTS] = {};
		SubpassDesc m_subpasses[RenderPassDesc::MAX_SUBPASSES] = {};
		VkSubpassDependency m_dependencies[RenderPassDesc::MAX_DEPENDENCIES] = {};
		size_t m_hashValue;

		explicit RenderPassCompatDesc();
		// cleans up array elements past array count and precomputes a hash value
		void finalize();
	};

	struct CombinedGraphicsPipelineRenderPassDesc
	{
		GraphicsPipelineDesc m_graphicsPipelineDesc;
		RenderPassCompatDesc m_renderPassCompatDesc;
		uint32_t m_subpassIndex;
	};

	bool operator==(const RenderPassDesc &lhs, const RenderPassDesc &rhs);
	bool operator==(const RenderPassCompatDesc &lhs, const RenderPassCompatDesc &rhs);
	bool operator==(const GraphicsPipelineDesc &lhs, const GraphicsPipelineDesc &rhs);
	bool operator==(const ComputePipelineDesc &lhs, const ComputePipelineDesc &rhs);
	bool operator==(const CombinedGraphicsPipelineRenderPassDesc &lhs, const CombinedGraphicsPipelineRenderPassDesc &rhs);

	struct RenderPassDescHash { size_t operator()(const RenderPassDesc &value) const; };
	struct RenderPassCompatDescHash { size_t operator()(const RenderPassCompatDesc &value) const; };
	struct GraphicsPipelineDescHash { size_t operator()(const GraphicsPipelineDesc &value) const; };
	struct ComputePipelineDescHash { size_t operator()(const ComputePipelineDesc &value) const; };
	struct CombinedGraphicsPipelineRenderPassDescHash { size_t operator()(const CombinedGraphicsPipelineRenderPassDesc &value) const; };
}