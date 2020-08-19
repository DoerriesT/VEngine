#include "Initializers.h"
#include <cstring>
#include <cassert>

VEngine::gal::Barrier VEngine::gal::Initializers::imageBarrier(const Image *image, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter, const ImageSubresourceRange &subresourceRange)
{
	return { image, nullptr, stagesBefore, stagesAfter, stateBefore, stateAfter, nullptr, nullptr, subresourceRange, false, false };
}

VEngine::gal::Barrier VEngine::gal::Initializers::bufferBarrier(const Buffer *buffer, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter)
{
	return { nullptr, buffer, stagesBefore, stagesAfter, stateBefore, stateAfter, nullptr, nullptr };
}

void VEngine::gal::Initializers::submitSingleTimeCommands(Queue *queue, CommandList *cmdList)
{
	SubmitInfo submitInfo = {};
	submitInfo.m_commandListCount = 1;
	submitInfo.m_commandLists = &cmdList;
	queue->submit(1, &submitInfo);
	queue->waitIdle();
}

bool VEngine::gal::Initializers::isReadAccess(ResourceState state)
{
	switch (state)
	{
	case ResourceState::READ_IMAGE_HOST:
	case ResourceState::READ_BUFFER_HOST:
	case ResourceState::READ_DEPTH_STENCIL:
	case ResourceState::READ_DEPTH_STENCIL_FRAG_SHADER:
	case ResourceState::READ_TEXTURE:
	case ResourceState::READ_STORAGE_IMAGE:
	case ResourceState::READ_STORAGE_BUFFER:
	case ResourceState::READ_UNIFORM_BUFFER:
	case ResourceState::READ_VERTEX_BUFFER:
	case ResourceState::READ_INDEX_BUFFER:
	case ResourceState::READ_INDIRECT_BUFFER:
	case ResourceState::READ_BUFFER_TRANSFER:
	case ResourceState::READ_IMAGE_TRANSFER:
	case ResourceState::READ_WRITE_IMAGE_HOST:
	case ResourceState::READ_WRITE_BUFFER_HOST:
	case ResourceState::READ_WRITE_STORAGE_IMAGE:
	case ResourceState::READ_WRITE_STORAGE_BUFFER:
	case ResourceState::READ_WRITE_DEPTH_STENCIL:
	case ResourceState::PRESENT_IMAGE:
		return true;
	default:
		break;
	}
	return false;
}

bool VEngine::gal::Initializers::isWriteAccess(ResourceState state)
{
	switch (state)
	{
	case ResourceState::READ_WRITE_IMAGE_HOST:
	case ResourceState::READ_WRITE_BUFFER_HOST:
	case ResourceState::READ_WRITE_STORAGE_IMAGE:
	case ResourceState::READ_WRITE_STORAGE_BUFFER:
	case ResourceState::READ_WRITE_DEPTH_STENCIL:
	case ResourceState::WRITE_IMAGE_HOST:
	case ResourceState::WRITE_BUFFER_HOST:
	case ResourceState::WRITE_ATTACHMENT:
	case ResourceState::WRITE_STORAGE_IMAGE:
	case ResourceState::WRITE_STORAGE_BUFFER:
	case ResourceState::WRITE_BUFFER_TRANSFER:
	case ResourceState::WRITE_IMAGE_TRANSFER:
		return true;
	default:
		break;
	}
	return false;
}

uint32_t VEngine::gal::Initializers::getUsageFlags(ResourceState state)
{
	switch (state)
	{
	case ResourceState::UNDEFINED:
		return 0;

	case ResourceState::READ_IMAGE_HOST:
		return 0;

	case ResourceState::READ_BUFFER_HOST:
		return 0;

	case ResourceState::READ_DEPTH_STENCIL:
		return ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT_BIT;

	case ResourceState::READ_DEPTH_STENCIL_FRAG_SHADER:
		return ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT_BIT | ImageUsageFlagBits::SAMPLED_BIT;

	case ResourceState::READ_TEXTURE:
		return ImageUsageFlagBits::SAMPLED_BIT;

	case ResourceState::READ_STORAGE_IMAGE:
		return ImageUsageFlagBits::STORAGE_BIT;

	case ResourceState::READ_STORAGE_BUFFER:
		return BufferUsageFlagBits::STORAGE_BUFFER_BIT;

	case ResourceState::READ_UNIFORM_BUFFER:
		return BufferUsageFlagBits::UNIFORM_BUFFER_BIT;

	case ResourceState::READ_VERTEX_BUFFER:
		return BufferUsageFlagBits::VERTEX_BUFFER_BIT;

	case ResourceState::READ_INDEX_BUFFER:
		return BufferUsageFlagBits::INDEX_BUFFER_BIT;

	case ResourceState::READ_INDIRECT_BUFFER:
		return BufferUsageFlagBits::INDIRECT_BUFFER_BIT;

	case ResourceState::READ_BUFFER_TRANSFER:
		return BufferUsageFlagBits::TRANSFER_SRC_BIT;

	case ResourceState::READ_IMAGE_TRANSFER:
		return ImageUsageFlagBits::TRANSFER_SRC_BIT;

	case ResourceState::READ_WRITE_IMAGE_HOST:
		return 0;

	case ResourceState::READ_WRITE_BUFFER_HOST:
		return 0;

	case ResourceState::READ_WRITE_STORAGE_IMAGE:
		return ImageUsageFlagBits::STORAGE_BIT;

	case ResourceState::READ_WRITE_STORAGE_BUFFER:
		return BufferUsageFlagBits::STORAGE_BUFFER_BIT;

	case ResourceState::READ_WRITE_DEPTH_STENCIL:
		return ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT_BIT;

	case ResourceState::WRITE_IMAGE_HOST:
		return 0;

	case ResourceState::WRITE_BUFFER_HOST:
		return 0;

	case ResourceState::WRITE_ATTACHMENT:
		return ImageUsageFlagBits::COLOR_ATTACHMENT_BIT;

	case ResourceState::WRITE_STORAGE_IMAGE:
		return ImageUsageFlagBits::STORAGE_BIT;

	case ResourceState::WRITE_STORAGE_BUFFER:
		return BufferUsageFlagBits::STORAGE_BUFFER_BIT;

	case ResourceState::WRITE_BUFFER_TRANSFER:
		return BufferUsageFlagBits::TRANSFER_DST_BIT;

	case ResourceState::WRITE_IMAGE_TRANSFER:
		return ImageUsageFlagBits::TRANSFER_DST_BIT;

	case ResourceState::CLEAR_BUFFER:
		return BufferUsageFlagBits::CLEAR_BIT;

	case ResourceState::CLEAR_IMAGE:
		return ImageUsageFlagBits::CLEAR_BIT;

	case ResourceState::PRESENT_IMAGE:
		return 0;

	default:
		assert(false);
	}
	return 0;
}

bool VEngine::gal::Initializers::isDepthFormat(Format format)
{
	switch (format)
	{
	case Format::D16_UNORM:
	case Format::X8_D24_UNORM_PACK32:
	case Format::D32_SFLOAT:
	case Format::D16_UNORM_S8_UINT:
	case Format::D24_UNORM_S8_UINT:
	case Format::D32_SFLOAT_S8_UINT:
		return true;
	default:
		return false;
	}
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
	m_createInfo.m_viewportState.m_viewportCount = 1;
	m_createInfo.m_viewportState.m_viewports[0] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	m_createInfo.m_viewportState.m_scissors[0] = { {0, 0}, {1, 1} };
	m_createInfo.m_inputAssemblyState.m_primitiveTopology = PrimitiveTopology::TRIANGLE_LIST;
	m_createInfo.m_multiSampleState.m_rasterizationSamples = SampleCount::_1;
	m_createInfo.m_multiSampleState.m_sampleMask = 0xFFFFFFFF;
	m_createInfo.m_rasterizationState.m_lineWidth = 1.0f;
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

void VEngine::gal::GraphicsPipelineBuilder::setDynamicState(DynamicStateFlags dynamicStateFlags)
{
	m_createInfo.m_dynamicStateFlags = dynamicStateFlags;
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

VEngine::gal::DescriptorSetUpdate VEngine::gal::Initializers::samplerDescriptor(const Sampler *const *samplers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType::SAMPLER, samplers, nullptr, nullptr, nullptr };
}

VEngine::gal::DescriptorSetUpdate VEngine::gal::Initializers::sampledImage(const ImageView *const *images, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType::SAMPLED_IMAGE, nullptr, images, nullptr, nullptr };
}

VEngine::gal::DescriptorSetUpdate VEngine::gal::Initializers::storageImage(const ImageView *const *images, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType::STORAGE_IMAGE, nullptr, images, nullptr, nullptr };
}

VEngine::gal::DescriptorSetUpdate VEngine::gal::Initializers::uniformTexelBuffer(const BufferView *const *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType::UNIFORM_TEXEL_BUFFER, nullptr, nullptr, buffers, nullptr };
}

VEngine::gal::DescriptorSetUpdate VEngine::gal::Initializers::storageTexelBuffer(const BufferView *const *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType::STORAGE_TEXEL_BUFFER, nullptr, nullptr, buffers, nullptr };
}

VEngine::gal::DescriptorSetUpdate VEngine::gal::Initializers::uniformBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType::UNIFORM_BUFFER, nullptr, nullptr, nullptr, buffers };
}

VEngine::gal::DescriptorSetUpdate VEngine::gal::Initializers::storageBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType::STORAGE_BUFFER, nullptr, nullptr, nullptr, buffers };
}

VEngine::gal::ComputePipelineBuilder::ComputePipelineBuilder(ComputePipelineCreateInfo &createInfo)
	:m_createInfo(createInfo)
{
	memset(&m_createInfo, 0, sizeof(m_createInfo));
}

void VEngine::gal::ComputePipelineBuilder::setComputeShader(const char *path)
{
	strcpy_s(m_createInfo.m_computeShader.m_path, path);
}
