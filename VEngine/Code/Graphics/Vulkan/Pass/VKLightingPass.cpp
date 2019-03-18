#include "VKLightingPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"

VEngine::VKLightingPass::VKLightingPass(
	VKRenderResources *renderResources,
	uint32_t width,
	uint32_t height,
	size_t resourceIndex)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex)
{
	strcpy_s(m_pipelineDesc.m_computeShaderPath, "Resources/Shaders/lighting_comp.spv");

	m_pipelineDesc.m_layout.m_setLayoutCount = 2;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayouts[COMMON_SET_INDEX];
	m_pipelineDesc.m_layout.m_setLayouts[1] = m_renderResources->m_descriptorSetLayouts[LIGHTING_SET_INDEX];
}

void VEngine::VKLightingPass::addToGraph(FrameGraph::Graph &graph,
	FrameGraph::BufferHandle perFrameDataBufferHandle,
	FrameGraph::BufferHandle directionalLightDataBufferHandle,
	FrameGraph::BufferHandle pointLightDataBufferHandle,
	FrameGraph::BufferHandle shadowDataBufferHandle,
	FrameGraph::BufferHandle pointLightZBinsBufferHandle,
	FrameGraph::BufferHandle pointLightBitMaskBufferHandle,
	FrameGraph::ImageHandle depthTextureHandle,
	FrameGraph::ImageHandle albedoTextureHandle,
	FrameGraph::ImageHandle normalTextureHandle,
	FrameGraph::ImageHandle materialTextureHandle,
	FrameGraph::ImageHandle shadowTextureHandle,
	FrameGraph::ImageHandle lightTextureHandle)
{
	auto builder = graph.addComputePass("Lighting Pass", this, &m_pipelineDesc, FrameGraph::QueueType::GRAPHICS);

	// common set
	builder.readUniformBuffer(perFrameDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::PER_FRAME_DATA_BINDING);
	builder.readStorageBuffer(shadowDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::SHADOW_DATA_BINDING);
	builder.readStorageBuffer(directionalLightDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::DIRECTIONAL_LIGHT_DATA_BINDING);
	builder.readStorageBuffer(pointLightDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::POINT_LIGHT_DATA_BINDING);
	builder.readStorageBuffer(pointLightZBinsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::POINT_LIGHT_Z_BINS_BINDING);
	builder.readStorageBuffer(pointLightBitMaskBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], CommonSetBindings::POINT_LIGHT_BITMASK_BINDING);

	// lighting set
	builder.readTexture(depthTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler, m_renderResources->m_descriptorSets[m_resourceIndex][LIGHTING_SET_INDEX], LightingSetBindings::G_BUFFER_TEXTURES_BINDING, 0);
	builder.readTexture(albedoTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler, m_renderResources->m_descriptorSets[m_resourceIndex][LIGHTING_SET_INDEX], LightingSetBindings::G_BUFFER_TEXTURES_BINDING, 1);
	builder.readTexture(normalTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler, m_renderResources->m_descriptorSets[m_resourceIndex][LIGHTING_SET_INDEX], LightingSetBindings::G_BUFFER_TEXTURES_BINDING, 2);
	builder.readTexture(materialTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler, m_renderResources->m_descriptorSets[m_resourceIndex][LIGHTING_SET_INDEX], LightingSetBindings::G_BUFFER_TEXTURES_BINDING, 3);
	builder.readTexture(shadowTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_shadowSampler, m_renderResources->m_descriptorSets[m_resourceIndex][LIGHTING_SET_INDEX], LightingSetBindings::SHADOW_TEXTURE_BINDING);
	builder.writeStorageImage(lightTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][LIGHTING_SET_INDEX], LightingSetBindings::RESULT_IMAGE_BINDING);
}


void VEngine::VKLightingPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][COMMON_SET_INDEX], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 1, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][LIGHTING_SET_INDEX], 0, nullptr);

	VKUtility::dispatchComputeHelper(cmdBuf, m_width, m_height, 1, 8, 8, 1);
}
