#include "VKPipeline.h"
#include "Utility/Utility.h"
#include <cassert>

const VkPipelineColorBlendAttachmentState VEngine::GraphicsPipelineDesc::s_defaultBlendAttachment =
{
	VK_FALSE,
	VK_BLEND_FACTOR_ZERO,
	VK_BLEND_FACTOR_ZERO,
	VK_BLEND_OP_ADD,
	VK_BLEND_FACTOR_ZERO,
	VK_BLEND_FACTOR_ZERO,
	VK_BLEND_OP_ADD,
	VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
};

VEngine::GraphicsPipelineDesc::GraphicsPipelineDesc()
{
	memset(this, 0, sizeof(*this));
	m_viewportState.m_viewportCount = 1;
	m_viewportState.m_viewports[0] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	m_viewportState.m_scissors[0] = { {0, 0}, {1, 1} };
	m_inputAssemblyState.m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	m_multiSampleState.m_sampleMask = 0xFFFFFFFF;
	m_rasterizationState.m_lineWidth = 1.0f;
}

void VEngine::GraphicsPipelineDesc::setVertexShader(const char *path, size_t specializationConstCount, const SpecEntry *specializationConsts)
{
	setShader(m_vertexShader, path, specializationConstCount, specializationConsts);
}

void VEngine::GraphicsPipelineDesc::setTessControlShader(const char *path, size_t specializationConstCount, const SpecEntry *specializationConsts)
{
	setShader(m_tessControlShader, path, specializationConstCount, specializationConsts);
}

void VEngine::GraphicsPipelineDesc::setTessEvalShader(const char *path, size_t specializationConstCount, const SpecEntry *specializationConsts)
{
	setShader(m_tessEvalShader, path, specializationConstCount, specializationConsts);
}

void VEngine::GraphicsPipelineDesc::setGeometryShader(const char *path, size_t specializationConstCount, const SpecEntry *specializationConsts)
{
	setShader(m_geometryShader, path, specializationConstCount, specializationConsts);
}

void VEngine::GraphicsPipelineDesc::setFragmentShader(const char *path, size_t specializationConstCount, const SpecEntry *specializationConsts)
{
	setShader(m_fragmentShader, path, specializationConstCount, specializationConsts);
}

void VEngine::GraphicsPipelineDesc::setVertexBindingDescriptions(size_t count, const VkVertexInputBindingDescription *bindingDescs)
{
	assert(count < MAX_VERTEX_BINDING_DESCRIPTIONS);
	m_vertexInputState.m_vertexBindingDescriptionCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_vertexInputState.m_vertexBindingDescriptions[i] = bindingDescs[i];
	}
}

void VEngine::GraphicsPipelineDesc::setVertexBindingDescription(const VkVertexInputBindingDescription &bindingDesc)
{
	m_vertexInputState.m_vertexBindingDescriptionCount = 1;
	m_vertexInputState.m_vertexBindingDescriptions[0] = bindingDesc;
}

void VEngine::GraphicsPipelineDesc::setVertexAttributeDescriptions(size_t count, const VkVertexInputAttributeDescription *attributeDescs)
{
	assert(count < MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS);
	m_vertexInputState.m_vertexAttributeDescriptionCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_vertexInputState.m_vertexAttributeDescriptions[i] = attributeDescs[i];
	}
}

void VEngine::GraphicsPipelineDesc::setVertexAttributeDescription(const VkVertexInputAttributeDescription &attributeDesc)
{
	m_vertexInputState.m_vertexAttributeDescriptionCount = 1;
	m_vertexInputState.m_vertexAttributeDescriptions[0] = attributeDesc;
}

void VEngine::GraphicsPipelineDesc::setInputAssemblyState(VkPrimitiveTopology topology, bool primitiveRestartEnable)
{
	m_inputAssemblyState.m_primitiveTopology = topology;
	m_inputAssemblyState.m_primitiveRestartEnable = primitiveRestartEnable;
}

void VEngine::GraphicsPipelineDesc::setTesselationState(uint32_t patchControlPoints)
{
	m_tesselationState.m_patchControlPoints = patchControlPoints;
}

void VEngine::GraphicsPipelineDesc::setViewportScissors(size_t count, const VkViewport *viewports, const VkRect2D *scissors)
{
	assert(count < MAX_VIEWPORTS);
	m_viewportState.m_viewportCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_viewportState.m_viewports[i] = viewports[i];
		m_viewportState.m_scissors[i] = scissors[i];
	}
}

void VEngine::GraphicsPipelineDesc::setViewportScissor(const VkViewport &viewport, const VkRect2D &scissor)
{
	m_viewportState.m_viewportCount = 1;
	m_viewportState.m_viewports[0] = viewport;
	m_viewportState.m_scissors[0] = scissor;
}

void VEngine::GraphicsPipelineDesc::setDepthClampEnable(bool depthClampEnable)
{
	m_rasterizationState.m_depthClampEnable = depthClampEnable;
}

void VEngine::GraphicsPipelineDesc::setRasterizerDiscardEnable(bool rasterizerDiscardEnable)
{
	m_rasterizationState.m_rasterizerDiscardEnable = rasterizerDiscardEnable;
}

void VEngine::GraphicsPipelineDesc::setPolygonModeCullMode(VkPolygonMode polygonMode, VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	m_rasterizationState.m_polygonMode = polygonMode;
	m_rasterizationState.m_cullMode = cullMode;
	m_rasterizationState.m_frontFace = frontFace;
}

void VEngine::GraphicsPipelineDesc::setDepthBias(bool enable, float constantFactor, float clamp, float slopeFactor)
{
	m_rasterizationState.m_depthBiasEnable = enable;
	m_rasterizationState.m_depthBiasConstantFactor = constantFactor;
	m_rasterizationState.m_depthBiasClamp = clamp;
	m_rasterizationState.m_depthBiasSlopeFactor = slopeFactor;
}

void VEngine::GraphicsPipelineDesc::setLineWidth(float lineWidth)
{
	m_rasterizationState.m_lineWidth = lineWidth;
}

void VEngine::GraphicsPipelineDesc::setMultisampleState(VkSampleCountFlagBits rasterizationSamples, bool sampleShadingEnable, float minSampleShading, VkSampleMask sampleMask, bool alphaToCoverageEnable, bool alphaToOneEnable)
{
	m_multiSampleState.m_rasterizationSamples = rasterizationSamples;
	m_multiSampleState.m_sampleShadingEnable = sampleShadingEnable;
	m_multiSampleState.m_minSampleShading = minSampleShading;
	m_multiSampleState.m_sampleMask = sampleMask;
	m_multiSampleState.m_alphaToCoverageEnable = alphaToCoverageEnable;
	m_multiSampleState.m_alphaToOneEnable = alphaToOneEnable;
}

void VEngine::GraphicsPipelineDesc::setDepthTest(bool depthTestEnable, bool depthWriteEnable, VkCompareOp depthCompareOp)
{
	m_depthStencilState.m_depthTestEnable = depthTestEnable;
	m_depthStencilState.m_depthWriteEnable = depthWriteEnable;
	m_depthStencilState.m_depthCompareOp = depthCompareOp;
}

void VEngine::GraphicsPipelineDesc::setStencilTest(bool stencilTestEnable, const VkStencilOpState &front, const VkStencilOpState &back)
{
	m_depthStencilState.m_stencilTestEnable = stencilTestEnable;
	m_depthStencilState.m_front = front;
	m_depthStencilState.m_back = back;
}

void VEngine::GraphicsPipelineDesc::setDepthBoundsTest(bool depthBoundsTestEnable, float minDepthBounds, float maxDepthBounds)
{
	m_depthStencilState.m_depthBoundsTestEnable = depthBoundsTestEnable;
	m_depthStencilState.m_minDepthBounds = minDepthBounds;
	m_depthStencilState.m_maxDepthBounds = maxDepthBounds;
}

void VEngine::GraphicsPipelineDesc::setBlendStateLogicOp(bool logicOpEnable, VkLogicOp logicOp)
{
	m_blendState.m_logicOpEnable = logicOpEnable;
	m_blendState.m_logicOp = logicOp;
}

void VEngine::GraphicsPipelineDesc::setBlendConstants(float blendConst0, float blendConst1, float blendConst2, float blendConst3)
{
	m_blendState.m_blendConstants[0] = blendConst0;
	m_blendState.m_blendConstants[1] = blendConst1;
	m_blendState.m_blendConstants[2] = blendConst2;
	m_blendState.m_blendConstants[3] = blendConst3;
}

void VEngine::GraphicsPipelineDesc::setColorBlendAttachments(size_t count, const VkPipelineColorBlendAttachmentState *colorBlendAttachments)
{
	assert(count < MAX_COLOR_BLEND_ATTACHMENT_STATES);
	m_blendState.m_attachmentCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_blendState.m_attachments[i] = colorBlendAttachments[i];
	}
}

void VEngine::GraphicsPipelineDesc::setColorBlendAttachment(const VkPipelineColorBlendAttachmentState &colorBlendAttachment)
{
	m_blendState.m_attachmentCount = 1;
	m_blendState.m_attachments[0] = colorBlendAttachment;
}

void VEngine::GraphicsPipelineDesc::setDynamicState(size_t count, const VkDynamicState *dynamicState)
{
	assert(count < MAX_DYNAMIC_STATES);
	m_dynamicState.m_dynamicStateCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_dynamicState.m_dynamicStates[i] = dynamicState[i];
	}
}

void VEngine::GraphicsPipelineDesc::setDynamicState(VkDynamicState dynamicState)
{
	m_dynamicState.m_dynamicStateCount = 1;
	m_dynamicState.m_dynamicStates[0] = dynamicState;
}

void VEngine::GraphicsPipelineDesc::finalize()
{
	m_hashValue = 0;
	for (size_t i = 0; i < sizeof(*this); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

void VEngine::GraphicsPipelineDesc::setShader(ShaderStageDesc &shader, const char *path, size_t specializationConstCount, const SpecEntry *specializationConsts)
{
	assert(specializationConstCount < ShaderStageDesc::MAX_SPECIALIZATION_ENTRY_COUNT);

	strcpy_s(shader.m_path, path);

	shader.m_specConstCount = specializationConstCount;
	uint32_t curOffset = 0;
	for (size_t i = 0; i < specializationConstCount; ++i)
	{
		shader.m_specializationEntries[i] = { specializationConsts[i].m_constantID, curOffset, 4 };
		memcpy(&shader.m_specializationData[curOffset], &specializationConsts[i].m_value, 4);
		curOffset += 4;
	}
}

VEngine::ComputePipelineDesc::ComputePipelineDesc()
{
	memset(this, 0, sizeof(*this));
}

void VEngine::ComputePipelineDesc::setComputeShader(const char *path, size_t specializationConstCount, const SpecEntry *specializationConsts)
{
	assert(specializationConstCount < ShaderStageDesc::MAX_SPECIALIZATION_ENTRY_COUNT);

	strcpy_s(m_computeShader.m_path, path);

	m_computeShader.m_specConstCount = specializationConstCount;
	uint32_t curOffset = 0;
	for (size_t i = 0; i < specializationConstCount; ++i)
	{
		m_computeShader.m_specializationEntries[i] = { specializationConsts[i].m_constantID, curOffset, 4 };
		memcpy(&m_computeShader.m_specializationData[curOffset], &specializationConsts[i].m_value, 4);
		curOffset += 4;
	}
}

void VEngine::ComputePipelineDesc::finalize()
{
	m_hashValue = 0;
	for (size_t i = 0; i < sizeof(*this); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

VEngine::RenderPassDesc::RenderPassDesc()
{
	memset(this, 0, sizeof(*this));
	m_subpassCount = 1;
}

void VEngine::RenderPassDesc::finalize()
{
	m_hashValue = 0;
	for (size_t i = 0; i < sizeof(*this); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

VEngine::RenderPassCompatDesc::RenderPassCompatDesc()
{
	memset(this, 0, sizeof(*this));
}

void VEngine::RenderPassCompatDesc::finalize()
{
	m_hashValue = 0;
	for (size_t i = 0; i < sizeof(*this); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

bool VEngine::operator==(const RenderPassDesc &lhs, const RenderPassDesc &rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

bool VEngine::operator==(const RenderPassCompatDesc &lhs, const RenderPassCompatDesc &rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

bool VEngine::operator==(const GraphicsPipelineDesc &lhs, const GraphicsPipelineDesc &rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

bool VEngine::operator==(const ComputePipelineDesc &lhs, const ComputePipelineDesc &rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

bool VEngine::operator==(const CombinedGraphicsPipelineRenderPassDesc &lhs, const CombinedGraphicsPipelineRenderPassDesc &rhs)
{
	return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

size_t VEngine::RenderPassDescHash::operator()(const RenderPassDesc &value) const
{
	return value.m_hashValue;
}

size_t VEngine::RenderPassCompatDescHash::operator()(const RenderPassCompatDesc &value) const
{
	return value.m_hashValue;
}

size_t VEngine::GraphicsPipelineDescHash::operator()(const GraphicsPipelineDesc &value) const
{
	return value.m_hashValue;
}

size_t VEngine::ComputePipelineDescHash::operator()(const ComputePipelineDesc &value) const
{
	return value.m_hashValue;
}

size_t VEngine::CombinedGraphicsPipelineRenderPassDescHash::operator()(const CombinedGraphicsPipelineRenderPassDesc &value) const
{
	size_t result = value.m_graphicsPipelineDesc.m_hashValue;
	Utility::hashCombine(result, value.m_renderPassCompatDesc.m_hashValue);
	return result;
}

VEngine::ShaderStageDesc::ShaderStageDesc()
{
	memset(this, 0, sizeof(*this));
}
