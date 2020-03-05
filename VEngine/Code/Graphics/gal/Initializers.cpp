#include "Initializers.h"
#include <cstring>
#include <cassert>

VEngine::gal::Barrier VEngine::gal::Initializers::imageBarrier(const Image *image, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter)
{
	return { image, nullptr, stagesBefore, stagesAfter, stateBefore, stateAfter, nullptr, nullptr };
}

VEngine::gal::Barrier VEngine::gal::Initializers::bufferBarrier(const Buffer *buffer, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter)
{
	return { nullptr, buffer, stagesBefore, stagesAfter, stateBefore, stateAfter, nullptr, nullptr };
}

const VEngine::gal::PipelineColorBlendAttachmentState VEngine::gal::GraphicsPipelineBuilder::s_defaultBlendAttachment =
{
	false,
	BlendFactor::ZERO,
	BlendFactor::ZERO,
	BlendOp::ADD,
	BlendFactor::ZERO,
	BlendFactor::ZERO,
	BlendOp::ADD,
	(uint32_t)ColorComponentFlagBits::R_BIT | (uint32_t)ColorComponentFlagBits::G_BIT | (uint32_t)ColorComponentFlagBits::B_BIT | (uint32_t)ColorComponentFlagBits::A_BIT
};

VEngine::gal::GraphicsPipelineBuilder::GraphicsPipelineBuilder(GraphicsPipelineCreateInfo &createInfo)
	:m_createInfo(createInfo)
{
	memset(&m_createInfo, 0, sizeof(m_createInfo));
}

void VEngine::gal::GraphicsPipelineBuilder::setVertexShader(const char *path)
{
	strcpy_s(m_createInfo.m_vertexShader.m_path, path);
}

void VEngine::gal::GraphicsPipelineBuilder::setTessControlShader(const char *path)
{
	strcpy_s(m_createInfo.m_tessControlShader.m_path, path);
}

void VEngine::gal::GraphicsPipelineBuilder::setTessEvalShader(const char *path)
{
	strcpy_s(m_createInfo.m_tessEvalShader.m_path, path);
}

void VEngine::gal::GraphicsPipelineBuilder::setGeometryShader(const char *path)
{
	strcpy_s(m_createInfo.m_geometryShader.m_path, path);
}

void VEngine::gal::GraphicsPipelineBuilder::setFragmentShader(const char *path)
{
	strcpy_s(m_createInfo.m_fragmentShader.m_path, path);
}

void VEngine::gal::GraphicsPipelineBuilder::setVertexBindingDescriptions(size_t count, const VertexInputBindingDescription *bindingDescs)
{
	assert(count < VertexInputState::MAX_VERTEX_BINDING_DESCRIPTIONS);
	m_createInfo.m_vertexInputState.m_vertexBindingDescriptionCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_vertexInputState.m_vertexBindingDescriptions[i] = bindingDescs[i];
	}
}

void VEngine::gal::GraphicsPipelineBuilder::setVertexBindingDescription(const VertexInputBindingDescription &bindingDesc)
{
	m_createInfo.m_vertexInputState.m_vertexBindingDescriptionCount = 1;
	m_createInfo.m_vertexInputState.m_vertexBindingDescriptions[0] = bindingDesc;
}

void VEngine::gal::GraphicsPipelineBuilder::setVertexAttributeDescriptions(size_t count, const VertexInputAttributeDescription *attributeDescs)
{
	assert(count < VertexInputState::MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS);
	m_createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i] = attributeDescs[i];
	}
}

void VEngine::gal::GraphicsPipelineBuilder::setVertexAttributeDescription(const VertexInputAttributeDescription &attributeDesc)
{
	m_createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount = 1;
	m_createInfo.m_vertexInputState.m_vertexAttributeDescriptions[0] = attributeDesc;
}

void VEngine::gal::GraphicsPipelineBuilder::setInputAssemblyState(PrimitiveTopology topology, bool primitiveRestartEnable)
{
	m_createInfo.m_inputAssemblyState.m_primitiveTopology = topology;
	m_createInfo.m_inputAssemblyState.m_primitiveRestartEnable = primitiveRestartEnable;
}

void VEngine::gal::GraphicsPipelineBuilder::setTesselationState(uint32_t patchControlPoints)
{
	m_createInfo.m_tesselationState.m_patchControlPoints = patchControlPoints;
}

void VEngine::gal::GraphicsPipelineBuilder::setViewportScissors(size_t count, const Viewport *viewports, const Rect *scissors)
{
	assert(count < ViewportState::MAX_VIEWPORTS);
	m_createInfo.m_viewportState.m_viewportCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_viewportState.m_viewports[i] = viewports[i];
		m_createInfo.m_viewportState.m_scissors[i] = scissors[i];
	}
}

void VEngine::gal::GraphicsPipelineBuilder::setViewportScissor(const Viewport &viewport, const Rect &scissor)
{
	m_createInfo.m_viewportState.m_viewportCount = 1;
	m_createInfo.m_viewportState.m_viewports[0] = viewport;
	m_createInfo.m_viewportState.m_scissors[0] = scissor;
}

void VEngine::gal::GraphicsPipelineBuilder::setDepthClampEnable(bool depthClampEnable)
{
	m_createInfo.m_rasterizationState.m_depthClampEnable = depthClampEnable;
}

void VEngine::gal::GraphicsPipelineBuilder::setRasterizerDiscardEnable(bool rasterizerDiscardEnable)
{
	m_createInfo.m_rasterizationState.m_rasterizerDiscardEnable = rasterizerDiscardEnable;
}

void VEngine::gal::GraphicsPipelineBuilder::setPolygonModeCullMode(PolygonMode polygonMode, CullModeFlags cullMode, FrontFace frontFace)
{
	m_createInfo.m_rasterizationState.m_polygonMode = polygonMode;
	m_createInfo.m_rasterizationState.m_cullMode = cullMode;
	m_createInfo.m_rasterizationState.m_frontFace = frontFace;
}

void VEngine::gal::GraphicsPipelineBuilder::setDepthBias(bool enable, float constantFactor, float clamp, float slopeFactor)
{
	m_createInfo.m_rasterizationState.m_depthBiasEnable = enable;
	m_createInfo.m_rasterizationState.m_depthBiasConstantFactor = constantFactor;
	m_createInfo.m_rasterizationState.m_depthBiasClamp = clamp;
	m_createInfo.m_rasterizationState.m_depthBiasSlopeFactor = slopeFactor;
}

void VEngine::gal::GraphicsPipelineBuilder::setLineWidth(float lineWidth)
{
	m_createInfo.m_rasterizationState.m_lineWidth = lineWidth;
}

void VEngine::gal::GraphicsPipelineBuilder::setMultisampleState(SampleCount rasterizationSamples, bool sampleShadingEnable, float minSampleShading, uint32_t sampleMask, bool alphaToCoverageEnable, bool alphaToOneEnable)
{
	m_createInfo.m_multiSampleState.m_rasterizationSamples = rasterizationSamples;
	m_createInfo.m_multiSampleState.m_sampleShadingEnable = sampleShadingEnable;
	m_createInfo.m_multiSampleState.m_minSampleShading = minSampleShading;
	m_createInfo.m_multiSampleState.m_sampleMask = sampleMask;
	m_createInfo.m_multiSampleState.m_alphaToCoverageEnable = alphaToCoverageEnable;
	m_createInfo.m_multiSampleState.m_alphaToOneEnable = alphaToOneEnable;
}

void VEngine::gal::GraphicsPipelineBuilder::setDepthTest(bool depthTestEnable, bool depthWriteEnable, CompareOp depthCompareOp)
{
	m_createInfo.m_depthStencilState.m_depthTestEnable = depthTestEnable;
	m_createInfo.m_depthStencilState.m_depthWriteEnable = depthWriteEnable;
	m_createInfo.m_depthStencilState.m_depthCompareOp = depthCompareOp;
}

void VEngine::gal::GraphicsPipelineBuilder::setStencilTest(bool stencilTestEnable, const StencilOpState &front, const StencilOpState &back)
{
	m_createInfo.m_depthStencilState.m_stencilTestEnable = stencilTestEnable;
	m_createInfo.m_depthStencilState.m_front = front;
	m_createInfo.m_depthStencilState.m_back = back;
}

void VEngine::gal::GraphicsPipelineBuilder::setDepthBoundsTest(bool depthBoundsTestEnable, float minDepthBounds, float maxDepthBounds)
{
	m_createInfo.m_depthStencilState.m_depthBoundsTestEnable = depthBoundsTestEnable;
	m_createInfo.m_depthStencilState.m_minDepthBounds = minDepthBounds;
	m_createInfo.m_depthStencilState.m_maxDepthBounds = maxDepthBounds;
}

void VEngine::gal::GraphicsPipelineBuilder::setBlendStateLogicOp(bool logicOpEnable, LogicOp logicOp)
{
	m_createInfo.m_blendState.m_logicOpEnable = logicOpEnable;
	m_createInfo.m_blendState.m_logicOp = logicOp;
}

void VEngine::gal::GraphicsPipelineBuilder::setBlendConstants(float blendConst0, float blendConst1, float blendConst2, float blendConst3)
{
	m_createInfo.m_blendState.m_blendConstants[0] = blendConst0;
	m_createInfo.m_blendState.m_blendConstants[1] = blendConst1;
	m_createInfo.m_blendState.m_blendConstants[2] = blendConst2;
	m_createInfo.m_blendState.m_blendConstants[3] = blendConst3;
}

void VEngine::gal::GraphicsPipelineBuilder::setColorBlendAttachments(size_t count, const PipelineColorBlendAttachmentState *colorBlendAttachments)
{
	assert(count < 8);
	m_createInfo.m_blendState.m_attachmentCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_blendState.m_attachments[i] = colorBlendAttachments[i];
	}
}

void VEngine::gal::GraphicsPipelineBuilder::setColorBlendAttachment(const PipelineColorBlendAttachmentState &colorBlendAttachment)
{
	m_createInfo.m_blendState.m_attachmentCount = 1;
	m_createInfo.m_blendState.m_attachments[0] = colorBlendAttachment;
}

void VEngine::gal::GraphicsPipelineBuilder::setDynamicState(size_t count, const DynamicState *dynamicState)
{
	assert(count < DynamicStates::MAX_DYNAMIC_STATES);
	m_createInfo.m_dynamicState.m_dynamicStateCount = count;
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_dynamicState.m_dynamicStates[i] = dynamicState[i];
	}
}

void VEngine::gal::GraphicsPipelineBuilder::setDynamicState(DynamicState dynamicState)
{
	m_createInfo.m_dynamicState.m_dynamicStateCount = 1;
	m_createInfo.m_dynamicState.m_dynamicStates[0] = dynamicState;
}

void VEngine::gal::GraphicsPipelineBuilder::setColorAttachmentFormats(uint32_t count, Format *formats)
{
	assert(count < 8);
	m_createInfo.m_attachmentFormats.m_colorAttachmentCount = count;
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_attachmentFormats.m_colorAttachmentFormats[i] = formats[i];
	}
}

void VEngine::gal::GraphicsPipelineBuilder::setColorAttachmentFormat(Format format)
{
	m_createInfo.m_attachmentFormats.m_colorAttachmentCount = 1;
	m_createInfo.m_attachmentFormats.m_colorAttachmentFormats[0] = format;
}

void VEngine::gal::GraphicsPipelineBuilder::setDepthStencilAttachmentFormat(Format format)
{
	m_createInfo.m_attachmentFormats.m_depthStencilFormat = format;
}
