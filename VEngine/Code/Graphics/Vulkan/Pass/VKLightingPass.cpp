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
	
	m_pipelineDesc.m_layout.m_setLayoutCount = 1;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayout;
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

	builder.readUniformBuffer(perFrameDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex], 0);
	builder.readStorageBuffer(directionalLightDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex], 3);
	builder.readStorageBuffer(pointLightDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex], 4);
	builder.readStorageBuffer(shadowDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex], 5);
	builder.readStorageBuffer(pointLightZBinsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex], 6);
	builder.readStorageBuffer(pointLightBitMaskBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex], 7);
	builder.readTexture(shadowTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_shadowSampler, m_renderResources->m_descriptorSets[m_resourceIndex], 8);
	builder.readTexture(depthTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler,m_renderResources->m_descriptorSets[m_resourceIndex], 2, 0);
	builder.readTexture(albedoTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler,m_renderResources->m_descriptorSets[m_resourceIndex], 2, 1);
	builder.readTexture(normalTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler,m_renderResources->m_descriptorSets[m_resourceIndex], 2, 2);
	builder.readTexture(materialTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler,m_renderResources->m_descriptorSets[m_resourceIndex], 2, 3);

	builder.writeStorageImage(lightTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex], 1);
}


void VEngine::VKLightingPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex], 0, nullptr);

	VKUtility::dispatchComputeHelper(cmdBuf, m_width, m_height, 1, 8, 8, 1);
}
