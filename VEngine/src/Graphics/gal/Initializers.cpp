#include "Initializers.h"
#include <cstring>
#include <cassert>

using namespace VEngine::gal;

Barrier Initializers::imageBarrier(const Image *image, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter, const ImageSubresourceRange &subresourceRange)
{
	return { image, nullptr, stagesBefore, stagesAfter, stateBefore, stateAfter, nullptr, nullptr, subresourceRange, false, false };
}

Barrier Initializers::bufferBarrier(const Buffer *buffer, PipelineStageFlags stagesBefore, PipelineStageFlags stagesAfter, ResourceState stateBefore, ResourceState stateAfter)
{
	return { nullptr, buffer, stagesBefore, stagesAfter, stateBefore, stateAfter, nullptr, nullptr };
}

void Initializers::submitSingleTimeCommands(Queue *queue, CommandList *cmdList)
{
	SubmitInfo submitInfo = {};
	submitInfo.m_commandListCount = 1;
	submitInfo.m_commandLists = &cmdList;
	queue->submit(1, &submitInfo);
	queue->waitIdle();
}

bool Initializers::isReadAccess(ResourceState state)
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

bool Initializers::isWriteAccess(ResourceState state)
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

uint32_t Initializers::getUsageFlags(ResourceState state)
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

bool Initializers::isDepthFormat(Format format)
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

bool VEngine::gal::Initializers::isStencilFormat(Format format)
{
	switch (format)
	{
	case Format::S8_UINT:
	case Format::D16_UNORM_S8_UINT:
	case Format::D24_UNORM_S8_UINT:
	case Format::D32_SFLOAT_S8_UINT:
		return true;
	default:
		return false;
	}
}

VEngine::gal::Initializers::FormatInfo VEngine::gal::Initializers::getFormatInfo(Format format)
{
	switch (format)
	{
	case Format::UNDEFINED:
		return FormatInfo{};
	case Format::R4G4_UNORM_PACK8:
		return { 1, true, false };
	case Format::R4G4B4A4_UNORM_PACK16:
		return { 2, true, false };
	case Format::B4G4R4A4_UNORM_PACK16:
		return { 2, true, false };
	case Format::R5G6B5_UNORM_PACK16:
		return { 2, true, false };
	case Format::B5G6R5_UNORM_PACK16:
		return { 2, true, false };
	case Format::R5G5B5A1_UNORM_PACK16:
		return { 2, true, false };
	case Format::B5G5R5A1_UNORM_PACK16:
		return { 2, true, false };
	case Format::A1R5G5B5_UNORM_PACK16:
		return { 2, true, false };
	case Format::R8_UNORM:
		return { 1, true, false };
	case Format::R8_SNORM:
		return { 1, true, false };
	case Format::R8_USCALED:
		return { 1, true, false };
	case Format::R8_SSCALED:
		return { 1, true, false };
	case Format::R8_UINT:
		return { 1, false, false };
	case Format::R8_SINT:
		return { 1, false, false };
	case Format::R8_SRGB:
		return { 1, true, false };
	case Format::R8G8_UNORM:
		return { 2, true, false };
	case Format::R8G8_SNORM:
		return { 2, true, false };
	case Format::R8G8_USCALED:
		return { 2, true, false };
	case Format::R8G8_SSCALED:
		return { 2, true, false };
	case Format::R8G8_UINT:
		return { 2, false, false };
	case Format::R8G8_SINT:
		return { 2, false, false };
	case Format::R8G8_SRGB:
		return { 2, true, false };
	case Format::R8G8B8_UNORM:
		return { 3, true, false };
	case Format::R8G8B8_SNORM:
		return { 3, true, false };
	case Format::R8G8B8_USCALED:
		return { 3, true, false };
	case Format::R8G8B8_SSCALED:
		return { 3, true, false };
	case Format::R8G8B8_UINT:
		return { 3, false, false };
	case Format::R8G8B8_SINT:
		return { 3, false, false };
	case Format::R8G8B8_SRGB:
		return { 3, true, false };
	case Format::B8G8R8_UNORM:
		return { 3, true, false };
	case Format::B8G8R8_SNORM:
		return { 3, true, false };
	case Format::B8G8R8_USCALED:
		return { 3, true, false };
	case Format::B8G8R8_SSCALED:
		return { 3, true, false };
	case Format::B8G8R8_UINT:
		return { 3, false, false };
	case Format::B8G8R8_SINT:
		return { 3, false, false };
	case Format::B8G8R8_SRGB:
		return { 3, true, false };
	case Format::R8G8B8A8_UNORM:
		return { 4, true, false };
	case Format::R8G8B8A8_SNORM:
		return { 4, true, false };
	case Format::R8G8B8A8_USCALED:
		return { 4, true, false };
	case Format::R8G8B8A8_SSCALED:
		return { 4, true, false };
	case Format::R8G8B8A8_UINT:
		return { 4, false, false };
	case Format::R8G8B8A8_SINT:
		return { 4, false, false };
	case Format::R8G8B8A8_SRGB:
		return { 4, true, false };
	case Format::B8G8R8A8_UNORM:
		return { 4, true, false };
	case Format::B8G8R8A8_SNORM:
		return { 4, true, false };
	case Format::B8G8R8A8_USCALED:
		return { 4, true, false };
	case Format::B8G8R8A8_SSCALED:
		return { 4, true, false };
	case Format::B8G8R8A8_UINT:
		return { 4, false, false };
	case Format::B8G8R8A8_SINT:
		return { 4, false, false };
	case Format::B8G8R8A8_SRGB:
		return { 4, true, false };
	case Format::A8B8G8R8_UNORM_PACK32:
		return { 4, true, false };
	case Format::A8B8G8R8_SNORM_PACK32:
		return { 4, true, false };
	case Format::A8B8G8R8_USCALED_PACK32:
		return { 4, true, false };
	case Format::A8B8G8R8_SSCALED_PACK32:
		return { 4, true, false };
	case Format::A8B8G8R8_UINT_PACK32:
		return { 4, false, false };
	case Format::A8B8G8R8_SINT_PACK32:
		return { 4, false, false };
	case Format::A8B8G8R8_SRGB_PACK32:
		return { 4, true, false };
	case Format::A2R10G10B10_UNORM_PACK32:
		return { 4, true, false };
	case Format::A2R10G10B10_SNORM_PACK32:
		return { 4, true, false };
	case Format::A2R10G10B10_USCALED_PACK32:
		return { 4, true, false };
	case Format::A2R10G10B10_SSCALED_PACK32:
		return { 4, true, false };
	case Format::A2R10G10B10_UINT_PACK32:
		return { 4, false, false };
	case Format::A2R10G10B10_SINT_PACK32:
		return { 4, false, false };
	case Format::A2B10G10R10_UNORM_PACK32:
		return { 4, true, false };
	case Format::A2B10G10R10_SNORM_PACK32:
		return { 4, true, false };
	case Format::A2B10G10R10_USCALED_PACK32:
		return { 4, true, false };
	case Format::A2B10G10R10_SSCALED_PACK32:
		return { 4, true, false };
	case Format::A2B10G10R10_UINT_PACK32:
		return { 4, false, false };
	case Format::A2B10G10R10_SINT_PACK32:
		return { 4, false, false };
	case Format::R16_UNORM:
		return { 2, true, false };
	case Format::R16_SNORM:
		return { 2, true, false };
	case Format::R16_USCALED:
		return { 2, true, false };
	case Format::R16_SSCALED:
		return { 2, true, false };
	case Format::R16_UINT:
		return { 2, false, false };
	case Format::R16_SINT:
		return { 2, false, false };
	case Format::R16_SFLOAT:
		return { 2, true, false };
	case Format::R16G16_UNORM:
		return { 4, true, false };
	case Format::R16G16_SNORM:
		return { 4, true, false };
	case Format::R16G16_USCALED:
		return { 4, true, false };
	case Format::R16G16_SSCALED:
		return { 4, true, false };
	case Format::R16G16_UINT:
		return { 4, false, false };
	case Format::R16G16_SINT:
		return { 4, false, false };
	case Format::R16G16_SFLOAT:
		return { 4, true, false };
	case Format::R16G16B16_UNORM:
		return { 6, true, false };
	case Format::R16G16B16_SNORM:
		return { 6, true, false };
	case Format::R16G16B16_USCALED:
		return { 6, true, false };
	case Format::R16G16B16_SSCALED:
		return { 6, true, false };
	case Format::R16G16B16_UINT:
		return { 6, false, false };
	case Format::R16G16B16_SINT:
		return { 6, false, false };
	case Format::R16G16B16_SFLOAT:
		return { 6, true, false };
	case Format::R16G16B16A16_UNORM:
		return { 8, true, false };
	case Format::R16G16B16A16_SNORM:
		return { 8, true, false };
	case Format::R16G16B16A16_USCALED:
		return { 8, true, false };
	case Format::R16G16B16A16_SSCALED:
		return { 8, true, false };
	case Format::R16G16B16A16_UINT:
		return { 8, false, false };
	case Format::R16G16B16A16_SINT:
		return { 8, false, false };
	case Format::R16G16B16A16_SFLOAT:
		return { 8, true, false };
	case Format::R32_UINT:
		return { 4, false, false };
	case Format::R32_SINT:
		return { 4, false, false };
	case Format::R32_SFLOAT:
		return { 4, true, false };
	case Format::R32G32_UINT:
		return { 8, false, false };
	case Format::R32G32_SINT:
		return { 8, false, false };
	case Format::R32G32_SFLOAT:
		return { 8, true, false };
	case Format::R32G32B32_UINT:
		return { 12, false, false };
	case Format::R32G32B32_SINT:
		return { 12, false, false };
	case Format::R32G32B32_SFLOAT:
		return { 12, true, false };
	case Format::R32G32B32A32_UINT:
		return { 16, false, false };
	case Format::R32G32B32A32_SINT:
		return { 16, false, false };
	case Format::R32G32B32A32_SFLOAT:
		return { 16, true, false };
	case Format::R64_UINT:
		return { 8, false, false };
	case Format::R64_SINT:
		return { 8, false, false };
	case Format::R64_SFLOAT:
		return { 8, true, false };
	case Format::R64G64_UINT:
		return { 16, false, false };
	case Format::R64G64_SINT:
		return { 16, false, false };
	case Format::R64G64_SFLOAT:
		return { 16, true, false };
	case Format::R64G64B64_UINT:
		return { 24, false, false };
	case Format::R64G64B64_SINT:
		return { 24, false, false };
	case Format::R64G64B64_SFLOAT:
		return { 24, true, false };
	case Format::R64G64B64A64_UINT:
		return { 32, false, false };
	case Format::R64G64B64A64_SINT:
		return { 32, false, false };
	case Format::R64G64B64A64_SFLOAT:
		return { 32, true, false };
	case Format::B10G11R11_UFLOAT_PACK32:
		return { 4, true, false };
	case Format::E5B9G9R9_UFLOAT_PACK32:
		return { 4, true, false };
	case Format::D16_UNORM:
		return { 2, true, false };
	case Format::X8_D24_UNORM_PACK32:
		return { 4, true, false };
	case Format::D32_SFLOAT:
		return { 4, true, false };
	case Format::S8_UINT:
		return { 1, false, false };
	case Format::D16_UNORM_S8_UINT:
		return { 3, true, false };
	case Format::D24_UNORM_S8_UINT:
		return { 4, true, false };
	case Format::D32_SFLOAT_S8_UINT:
		return { 5, true, false };
	case Format::BC1_RGB_UNORM_BLOCK:
		return { 8, true, true };
	case Format::BC1_RGB_SRGB_BLOCK:
		return { 8, true, true };
	case Format::BC1_RGBA_UNORM_BLOCK:
		return { 8, true, true };
	case Format::BC1_RGBA_SRGB_BLOCK:
		return { 8, true, true };
	case Format::BC2_UNORM_BLOCK:
		return { 16, true, true };
	case Format::BC2_SRGB_BLOCK:
		return { 16, true, true };
	case Format::BC3_UNORM_BLOCK:
		return { 16, true, true };
	case Format::BC3_SRGB_BLOCK:
		return { 16, true, true };
	case Format::BC4_UNORM_BLOCK:
		return { 8, true, true };
	case Format::BC4_SNORM_BLOCK:
		return { 8, true, true };
	case Format::BC5_UNORM_BLOCK:
		return { 16, true, true };
	case Format::BC5_SNORM_BLOCK:
		return { 16, true, true };
	case Format::BC6H_UFLOAT_BLOCK:
		return { 16, true, true };
	case Format::BC6H_SFLOAT_BLOCK:
		return { 16, true, true };
	case Format::BC7_UNORM_BLOCK:
		return { 16, true, true };
	case Format::BC7_SRGB_BLOCK:
		return { 16, true, true };
	default:
		assert(false);
		break;
	}
	return FormatInfo{};
}

const PipelineColorBlendAttachmentState GraphicsPipelineBuilder::s_defaultBlendAttachment =
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

GraphicsPipelineBuilder::GraphicsPipelineBuilder(GraphicsPipelineCreateInfo &createInfo)
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

void GraphicsPipelineBuilder::setVertexShader(const char *path)
{
	strcpy_s(m_createInfo.m_vertexShader.m_path, path);
}

void GraphicsPipelineBuilder::setTessControlShader(const char *path)
{
	strcpy_s(m_createInfo.m_tessControlShader.m_path, path);
}

void GraphicsPipelineBuilder::setTessEvalShader(const char *path)
{
	strcpy_s(m_createInfo.m_tessEvalShader.m_path, path);
}

void GraphicsPipelineBuilder::setGeometryShader(const char *path)
{
	strcpy_s(m_createInfo.m_geometryShader.m_path, path);
}

void GraphicsPipelineBuilder::setFragmentShader(const char *path)
{
	strcpy_s(m_createInfo.m_fragmentShader.m_path, path);
}

void GraphicsPipelineBuilder::setVertexBindingDescriptions(size_t count, const VertexInputBindingDescription *bindingDescs)
{
	assert(count < VertexInputState::MAX_VERTEX_BINDING_DESCRIPTIONS);
	m_createInfo.m_vertexInputState.m_vertexBindingDescriptionCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_vertexInputState.m_vertexBindingDescriptions[i] = bindingDescs[i];
	}
}

void GraphicsPipelineBuilder::setVertexBindingDescription(const VertexInputBindingDescription &bindingDesc)
{
	m_createInfo.m_vertexInputState.m_vertexBindingDescriptionCount = 1;
	m_createInfo.m_vertexInputState.m_vertexBindingDescriptions[0] = bindingDesc;
}

void GraphicsPipelineBuilder::setVertexAttributeDescriptions(size_t count, const VertexInputAttributeDescription *attributeDescs)
{
	assert(count < VertexInputState::MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS);
	m_createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_vertexInputState.m_vertexAttributeDescriptions[i] = attributeDescs[i];
	}
}

void GraphicsPipelineBuilder::setVertexAttributeDescription(const VertexInputAttributeDescription &attributeDesc)
{
	m_createInfo.m_vertexInputState.m_vertexAttributeDescriptionCount = 1;
	m_createInfo.m_vertexInputState.m_vertexAttributeDescriptions[0] = attributeDesc;
}

void GraphicsPipelineBuilder::setInputAssemblyState(PrimitiveTopology topology, bool primitiveRestartEnable)
{
	m_createInfo.m_inputAssemblyState.m_primitiveTopology = topology;
	m_createInfo.m_inputAssemblyState.m_primitiveRestartEnable = primitiveRestartEnable;
}

void GraphicsPipelineBuilder::setTesselationState(uint32_t patchControlPoints)
{
	m_createInfo.m_tesselationState.m_patchControlPoints = patchControlPoints;
}

void GraphicsPipelineBuilder::setViewportScissors(size_t count, const Viewport *viewports, const Rect *scissors)
{
	assert(count < ViewportState::MAX_VIEWPORTS);
	m_createInfo.m_viewportState.m_viewportCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_viewportState.m_viewports[i] = viewports[i];
		m_createInfo.m_viewportState.m_scissors[i] = scissors[i];
	}
}

void GraphicsPipelineBuilder::setViewportScissor(const Viewport &viewport, const Rect &scissor)
{
	m_createInfo.m_viewportState.m_viewportCount = 1;
	m_createInfo.m_viewportState.m_viewports[0] = viewport;
	m_createInfo.m_viewportState.m_scissors[0] = scissor;
}

void GraphicsPipelineBuilder::setDepthClampEnable(bool depthClampEnable)
{
	m_createInfo.m_rasterizationState.m_depthClampEnable = depthClampEnable;
}

void GraphicsPipelineBuilder::setRasterizerDiscardEnable(bool rasterizerDiscardEnable)
{
	m_createInfo.m_rasterizationState.m_rasterizerDiscardEnable = rasterizerDiscardEnable;
}

void GraphicsPipelineBuilder::setPolygonModeCullMode(PolygonMode polygonMode, CullModeFlags cullMode, FrontFace frontFace)
{
	m_createInfo.m_rasterizationState.m_polygonMode = polygonMode;
	m_createInfo.m_rasterizationState.m_cullMode = cullMode;
	m_createInfo.m_rasterizationState.m_frontFace = frontFace;
}

void GraphicsPipelineBuilder::setDepthBias(bool enable, float constantFactor, float clamp, float slopeFactor)
{
	m_createInfo.m_rasterizationState.m_depthBiasEnable = enable;
	m_createInfo.m_rasterizationState.m_depthBiasConstantFactor = constantFactor;
	m_createInfo.m_rasterizationState.m_depthBiasClamp = clamp;
	m_createInfo.m_rasterizationState.m_depthBiasSlopeFactor = slopeFactor;
}

void GraphicsPipelineBuilder::setLineWidth(float lineWidth)
{
	m_createInfo.m_rasterizationState.m_lineWidth = lineWidth;
}

void GraphicsPipelineBuilder::setMultisampleState(SampleCount rasterizationSamples, bool sampleShadingEnable, float minSampleShading, uint32_t sampleMask, bool alphaToCoverageEnable, bool alphaToOneEnable)
{
	m_createInfo.m_multiSampleState.m_rasterizationSamples = rasterizationSamples;
	m_createInfo.m_multiSampleState.m_sampleShadingEnable = sampleShadingEnable;
	m_createInfo.m_multiSampleState.m_minSampleShading = minSampleShading;
	m_createInfo.m_multiSampleState.m_sampleMask = sampleMask;
	m_createInfo.m_multiSampleState.m_alphaToCoverageEnable = alphaToCoverageEnable;
	m_createInfo.m_multiSampleState.m_alphaToOneEnable = alphaToOneEnable;
}

void GraphicsPipelineBuilder::setDepthTest(bool depthTestEnable, bool depthWriteEnable, CompareOp depthCompareOp)
{
	m_createInfo.m_depthStencilState.m_depthTestEnable = depthTestEnable;
	m_createInfo.m_depthStencilState.m_depthWriteEnable = depthWriteEnable;
	m_createInfo.m_depthStencilState.m_depthCompareOp = depthCompareOp;
}

void GraphicsPipelineBuilder::setStencilTest(bool stencilTestEnable, const StencilOpState &front, const StencilOpState &back)
{
	m_createInfo.m_depthStencilState.m_stencilTestEnable = stencilTestEnable;
	m_createInfo.m_depthStencilState.m_front = front;
	m_createInfo.m_depthStencilState.m_back = back;
}

void GraphicsPipelineBuilder::setDepthBoundsTest(bool depthBoundsTestEnable, float minDepthBounds, float maxDepthBounds)
{
	m_createInfo.m_depthStencilState.m_depthBoundsTestEnable = depthBoundsTestEnable;
	m_createInfo.m_depthStencilState.m_minDepthBounds = minDepthBounds;
	m_createInfo.m_depthStencilState.m_maxDepthBounds = maxDepthBounds;
}

void GraphicsPipelineBuilder::setBlendStateLogicOp(bool logicOpEnable, LogicOp logicOp)
{
	m_createInfo.m_blendState.m_logicOpEnable = logicOpEnable;
	m_createInfo.m_blendState.m_logicOp = logicOp;
}

void GraphicsPipelineBuilder::setBlendConstants(float blendConst0, float blendConst1, float blendConst2, float blendConst3)
{
	m_createInfo.m_blendState.m_blendConstants[0] = blendConst0;
	m_createInfo.m_blendState.m_blendConstants[1] = blendConst1;
	m_createInfo.m_blendState.m_blendConstants[2] = blendConst2;
	m_createInfo.m_blendState.m_blendConstants[3] = blendConst3;
}

void GraphicsPipelineBuilder::setColorBlendAttachments(size_t count, const PipelineColorBlendAttachmentState *colorBlendAttachments)
{
	assert(count < 8);
	m_createInfo.m_blendState.m_attachmentCount = static_cast<uint32_t>(count);
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_blendState.m_attachments[i] = colorBlendAttachments[i];
	}
}

void GraphicsPipelineBuilder::setColorBlendAttachment(const PipelineColorBlendAttachmentState &colorBlendAttachment)
{
	m_createInfo.m_blendState.m_attachmentCount = 1;
	m_createInfo.m_blendState.m_attachments[0] = colorBlendAttachment;
}

void GraphicsPipelineBuilder::setDynamicState(DynamicStateFlags dynamicStateFlags)
{
	m_createInfo.m_dynamicStateFlags = dynamicStateFlags;
}

void GraphicsPipelineBuilder::setColorAttachmentFormats(uint32_t count, Format *formats)
{
	assert(count < 8);
	m_createInfo.m_attachmentFormats.m_colorAttachmentCount = count;
	for (size_t i = 0; i < count; ++i)
	{
		m_createInfo.m_attachmentFormats.m_colorAttachmentFormats[i] = formats[i];
	}
}

void GraphicsPipelineBuilder::setColorAttachmentFormat(Format format)
{
	m_createInfo.m_attachmentFormats.m_colorAttachmentCount = 1;
	m_createInfo.m_attachmentFormats.m_colorAttachmentFormats[0] = format;
}

void GraphicsPipelineBuilder::setDepthStencilAttachmentFormat(Format format)
{
	m_createInfo.m_attachmentFormats.m_depthStencilFormat = format;
}

ComputePipelineBuilder::ComputePipelineBuilder(ComputePipelineCreateInfo &createInfo)
	:m_createInfo(createInfo)
{
	memset(&m_createInfo, 0, sizeof(m_createInfo));
}

void ComputePipelineBuilder::setComputeShader(const char *path)
{
	strcpy_s(m_createInfo.m_computeShader.m_path, path);
}

DescriptorSetUpdate2 Initializers::sampler(const Sampler *const *samplers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::SAMPLER, samplers, nullptr, nullptr, nullptr };
}

DescriptorSetUpdate2 Initializers::texture(const ImageView *const *images, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::TEXTURE, nullptr, images, nullptr, nullptr };
}

DescriptorSetUpdate2 Initializers::depthStencilTexture(const ImageView *const *images, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::DEPTH_STENCIL_TEXTURE, nullptr, images, nullptr, nullptr };
}

DescriptorSetUpdate2 Initializers::rwTexture(const ImageView *const *images, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::RW_TEXTURE, nullptr, images, nullptr, nullptr };
}

DescriptorSetUpdate2 Initializers::typedBuffer(const BufferView *const *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::TYPED_BUFFER, nullptr, nullptr, buffers, nullptr };
}

DescriptorSetUpdate2 Initializers::rwTypedBuffer(const BufferView *const *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::RW_TYPED_BUFFER, nullptr, nullptr, buffers, nullptr };
}

DescriptorSetUpdate2 Initializers::constantBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::CONSTANT_BUFFER, nullptr, nullptr, nullptr, buffers };
}

DescriptorSetUpdate2 Initializers::byteBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::BYTE_BUFFER, nullptr, nullptr, nullptr, buffers };
}

DescriptorSetUpdate2 Initializers::rwByteBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::RW_BYTE_BUFFER, nullptr, nullptr, nullptr, buffers };
}

DescriptorSetUpdate2 Initializers::structuredBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::STRUCTURED_BUFFER, nullptr, nullptr, nullptr, buffers };
}

DescriptorSetUpdate2 Initializers::rwStructuredBuffer(const DescriptorBufferInfo *buffers, uint32_t binding, uint32_t arrayElement, uint32_t count)
{
	return { binding, arrayElement, count, DescriptorType2::RW_STRUCTURED_BUFFER, nullptr, nullptr, nullptr, buffers };
}
