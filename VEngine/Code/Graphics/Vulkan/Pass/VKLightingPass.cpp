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
	
	m_pipelineDesc.m_layout.m_setLayoutCount = 4;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_perFrameDataDescriptorSetLayout;
	m_pipelineDesc.m_layout.m_setLayouts[1] = m_renderResources->m_lightingInputDescriptorSetLayout;
	m_pipelineDesc.m_layout.m_setLayouts[2] = m_renderResources->m_lightDataDescriptorSetLayout;
	m_pipelineDesc.m_layout.m_setLayouts[3] = m_renderResources->m_lightBitMaskDescriptorSetLayout;
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) };
}

void VEngine::VKLightingPass::addToGraph(FrameGraph::Graph &graph,
	FrameGraph::BufferHandle perFrameDataBufferHandle,
	FrameGraph::BufferHandle directionalLightDataBufferHandle,
	FrameGraph::BufferHandle pointLightDataBufferHandle,
	FrameGraph::BufferHandle spotLightDataBufferHandle,
	FrameGraph::BufferHandle shadowDataBufferHandle,
	FrameGraph::BufferHandle pointLightZBinsBufferHandle,
	FrameGraph::BufferHandle pointLightBitMaskBufferHandle,
	FrameGraph::ImageHandle depthTextureHandle,
	FrameGraph::ImageHandle albedoTextureHandle,
	FrameGraph::ImageHandle normalTextureHandle,
	FrameGraph::ImageHandle materialTextureHandle,
	FrameGraph::ImageHandle shadowTextureHandle,
	FrameGraph::ImageHandle &lightTextureHandle)
{
	auto builder = graph.addComputePass("Lighting Pass", this, &m_pipelineDesc, FrameGraph::QueueType::GRAPHICS);

	if (!lightTextureHandle)
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "LightTexture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		lightTextureHandle = builder.createImage(desc);
	}

	builder.readUniformBuffer(perFrameDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_perFrameDataDescriptorSets[m_resourceIndex], 0);

	builder.readStorageBuffer(directionalLightDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_lightDataDescriptorSets[m_resourceIndex], 0);

	builder.readStorageBuffer(pointLightDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_lightDataDescriptorSets[m_resourceIndex], 1);

	builder.readStorageBuffer(spotLightDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_lightDataDescriptorSets[m_resourceIndex], 2);

	builder.readStorageBuffer(shadowDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_lightDataDescriptorSets[m_resourceIndex], 3);

	builder.readStorageBuffer(pointLightZBinsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_lightDataDescriptorSets[m_resourceIndex], 4);

	builder.readTexture(shadowTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_shadowSampler,
		m_renderResources->m_lightDataDescriptorSets[m_resourceIndex], 5);

	builder.readStorageBuffer(pointLightBitMaskBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_lightBitMaskDescriptorSets[m_resourceIndex], 0);

	builder.writeStorageImage(lightTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		m_renderResources->m_lightingInputDescriptorSets[m_resourceIndex], 0);

	builder.readTexture(depthTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler,
		m_renderResources->m_lightingInputDescriptorSets[m_resourceIndex], 1, 0);

	builder.readTexture(albedoTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler,
		m_renderResources->m_lightingInputDescriptorSets[m_resourceIndex], 1, 1);

	builder.readTexture(normalTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler,
		m_renderResources->m_lightingInputDescriptorSets[m_resourceIndex], 1, 2);

	builder.readTexture(materialTextureHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSampler,
		m_renderResources->m_lightingInputDescriptorSets[m_resourceIndex], 1, 3);
}


void VEngine::VKLightingPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &m_renderResources->m_perFrameDataDescriptorSets[m_resourceIndex], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 1, 1, &m_renderResources->m_lightingInputDescriptorSets[m_resourceIndex], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 2, 1, &m_renderResources->m_lightDataDescriptorSets[m_resourceIndex], 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 3, 1, &m_renderResources->m_lightBitMaskDescriptorSets[m_resourceIndex], 0, nullptr);

	uint32_t directionalLightCount = 1;
	vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &directionalLightCount);

	VKUtility::dispatchComputeHelper(cmdBuf, m_width, m_height, 1, 8, 8, 1);
}
