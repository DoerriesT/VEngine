#include "VKPipeline.h"
#include "Utility/Utility.h"

VEngine::VKRenderPassDescription::VKRenderPassDescription()
{
	memset(this, 0, sizeof(VKRenderPassDescription));
}

void VEngine::VKRenderPassDescription::finalize()
{
	//memset(m_attachments + m_attachmentCount, 0, (MAX_ATTACHMENTS - m_attachmentCount) * sizeof(m_attachments));
	//memset(m_inputAttachments + m_inputAttachmentCount, 0, (MAX_INPUT_ATTACHMENTS - m_inputAttachmentCount) * sizeof(m_inputAttachments));
	//memset(m_colorAttachments + m_colorAttachmentCount, 0, (MAX_COLOR_ATTACHMENTS - m_colorAttachmentCount) * sizeof(m_colorAttachments));
	//memset(m_resolveAttachments + m_resolveAttachmentCount, 0, (MAX_RESOLVE_ATTACHMENTS - m_resolveAttachmentCount) * sizeof(m_resolveAttachments));
	
	if (!m_depthStencilAttachmentPresent)
	{
		m_depthStencilAttachment = {};
	}

	m_hashValue = 0;

	for (size_t i = 0; i < sizeof(VKRenderPassDescription) - sizeof(m_hashValue); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

VEngine::VKGraphicsPipelineDescription::VKGraphicsPipelineDescription()
{
	memset(this, 0, sizeof(VKGraphicsPipelineDescription));
}

void VEngine::VKGraphicsPipelineDescription::finalize()
{
	auto cleanArray = [](uint8_t *dst, size_t offset, size_t totalSize)
	{
		memset(dst + offset, 0, totalSize - offset);
	};

	//cleanArray((uint8_t *)m_shaderStages.m_vertexShaderPath, strlen(m_shaderStages.m_vertexShaderPath), MAX_PATH_LENGTH + 1);
	//cleanArray((uint8_t *)m_shaderStages.m_tesselationControlShaderPath, strlen(m_shaderStages.m_tesselationControlShaderPath), MAX_PATH_LENGTH + 1);
	//cleanArray((uint8_t *)m_shaderStages.m_tesselationEvaluationShaderPath, strlen(m_shaderStages.m_tesselationEvaluationShaderPath), MAX_PATH_LENGTH + 1);
	//cleanArray((uint8_t *)m_shaderStages.m_geometryShaderPath, strlen(m_shaderStages.m_geometryShaderPath), MAX_PATH_LENGTH + 1);
	//cleanArray((uint8_t *)m_shaderStages.m_fragmentShaderPath, strlen(m_shaderStages.m_fragmentShaderPath), MAX_PATH_LENGTH + 1);
	//cleanArray((uint8_t *)&m_vertexInputState.m_vertexBindingDescriptions, m_vertexInputState.m_vertexBindingDescriptionCount * sizeof(m_vertexInputState.m_vertexBindingDescriptions[0]), sizeof(m_vertexInputState.m_vertexBindingDescriptions));
	//cleanArray((uint8_t *)&m_vertexInputState.m_vertexAttributeDescriptions, m_vertexInputState.m_vertexAttributeDescriptionCount * sizeof(m_vertexInputState.m_vertexAttributeDescriptions[0]), sizeof(m_vertexInputState.m_vertexAttributeDescriptions));
	//cleanArray((uint8_t *)&m_viewportState.m_viewports, m_viewportState.m_viewportCount * sizeof(m_viewportState.m_viewports[0]), sizeof(m_viewportState.m_viewports));
	//cleanArray((uint8_t *)&m_viewportState.m_scissors, m_viewportState.m_scissorCount * sizeof(m_viewportState.m_scissors[0]), sizeof(m_viewportState.m_scissors));
	//cleanArray((uint8_t *)&m_blendState.m_attachments, m_blendState.m_attachmentCount * sizeof(m_blendState.m_attachments[0]), sizeof(m_blendState.m_attachments));
	//cleanArray((uint8_t *)&m_dynamicState.m_dynamicStates, m_dynamicState.m_dynamicStateCount * sizeof(m_dynamicState.m_dynamicStates[0]), sizeof(m_dynamicState.m_dynamicStates));

	m_hashValue = 0;

	for (size_t i = 0; i < sizeof(VKGraphicsPipelineDescription); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

VEngine::VKComputePipelineDescription::VKComputePipelineDescription()
{
	memset(this, 0, sizeof(VKComputePipelineDescription));
}

void VEngine::VKComputePipelineDescription::finalize()
{
	size_t len = strlen(m_computeShaderPath);
	memset(m_computeShaderPath + len, 0, MAX_PATH_LENGTH + 1 - len);

	m_hashValue = 0;

	for (size_t i = 0; i < sizeof(VKComputePipelineDescription); ++i)
	{
		Utility::hashCombine(m_hashValue, reinterpret_cast<const char *>(this)[i]);
	}
}

size_t VEngine::VKRenderPassDescriptionHash::operator()(const VKRenderPassDescription &value) const
{
	return value.m_hashValue;
}

size_t VEngine::VKGraphicsPipelineDescriptionHash::operator()(const VKGraphicsPipelineDescription &value) const
{
	return value.m_hashValue;
}

size_t VEngine::VKComputePipelineDescriptionHash::operator()(const VKComputePipelineDescription &value) const
{
	return value.m_hashValue;
}

size_t VEngine::VKCombinedGraphicsPipelineRenderPassDescriptionHash::operator()(const VKCombinedGraphicsPipelineRenderPassDescription &value) const
{
	size_t result = value.m_graphicsPipelineDescription.m_hashValue;
	Utility::hashCombine(result, value.m_renderPassDescription.m_hashValue);
	return result;
}
