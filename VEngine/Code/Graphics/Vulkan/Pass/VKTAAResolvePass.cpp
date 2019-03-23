#include "VKTAAResolvePass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "GlobalVar.h"

struct PushConsts
{
	float bicubicSharpness;
	float temporalContrastThreshold;
	float lowStrengthAlpha;
	float highStrengthAlpha;
	float antiFlickeringAlpha;
};

VEngine::VKTAAResolvePass::VKTAAResolvePass(VKRenderResources *renderResources, uint32_t width, uint32_t height, size_t resourceIndex)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex)
{
	strcpy_s(m_pipelineDesc.m_computeShaderPath, "Resources/Shaders/taaResolve_comp.spv");

	m_pipelineDesc.m_layout.m_setLayoutCount = 2;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayouts[COMMON_SET_INDEX];
	m_pipelineDesc.m_layout.m_setLayouts[1] = m_renderResources->m_descriptorSetLayouts[TAA_RESOLVE_SET_INDEX];
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConsts) };
}

void VEngine::VKTAAResolvePass::addToGraph(FrameGraph::Graph &graph,
	FrameGraph::BufferHandle perFrameDataBufferHandle,
	FrameGraph::ImageHandle depthTextureHandle,
	FrameGraph::ImageHandle velocityTextureHandle,
	FrameGraph::ImageHandle taaHistoryTextureHandle,
	FrameGraph::ImageHandle lightTextureHandle,
	FrameGraph::ImageHandle taaResolveTextureHandle)
{
	auto builder = graph.addComputePass("TAA Resolve Pass", this, &m_pipelineDesc, FrameGraph::QueueType::GRAPHICS);

	// common set
	builder.readUniformBuffer(perFrameDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::PER_FRAME_DATA_BINDING);

	// taa resolve set
	builder.readTexture(depthTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSamplerClamp, m_renderResources->m_descriptorSets[m_resourceIndex][TAA_RESOLVE_SET_INDEX], TAAResolveSetBindings::DEPTH_TEXTURE_BINDING);
	builder.readTexture(velocityTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSamplerClamp, m_renderResources->m_descriptorSets[m_resourceIndex][TAA_RESOLVE_SET_INDEX], TAAResolveSetBindings::VELOCITY_TEXTURE_BINDING);
	builder.readTexture(taaHistoryTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_linearSamplerClamp, m_renderResources->m_descriptorSets[m_resourceIndex][TAA_RESOLVE_SET_INDEX], TAAResolveSetBindings::HISTORY_IMAGE_BINDING);
	builder.readTexture(lightTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSamplerClamp, m_renderResources->m_descriptorSets[m_resourceIndex][TAA_RESOLVE_SET_INDEX], TAAResolveSetBindings::SOURCE_TEXTURE_BINDING);
	builder.writeStorageImage(taaResolveTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][TAA_RESOLVE_SET_INDEX], TAAResolveSetBindings::RESULT_IMAGE_BINDING);
}

void VEngine::VKTAAResolvePass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry & registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 1, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][TAA_RESOLVE_SET_INDEX], 0, nullptr);

	PushConsts pushConsts;
	pushConsts.bicubicSharpness = g_TAABicubicSharpness;
	pushConsts.temporalContrastThreshold = g_TAATemporalContrastThreshold;
	pushConsts.lowStrengthAlpha = g_TAALowStrengthAlpha;
	pushConsts.highStrengthAlpha = g_TAAHighStrengthAlpha;
	pushConsts.antiFlickeringAlpha = g_TAAAntiFlickeringAlpha;

	vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

	VKUtility::dispatchComputeHelper(cmdBuf, m_width, m_height, 1, 8, 8, 1);
}
