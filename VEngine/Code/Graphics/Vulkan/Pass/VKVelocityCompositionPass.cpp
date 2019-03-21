#include "VKVelocityCompositionPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

struct PushConsts
{
	glm::mat4 reprojectionMatrix;
	glm::vec2 texelSize;
};

VEngine::VKVelocityCompositionPass::VKVelocityCompositionPass(VKRenderResources *renderResources,
	uint32_t width, 
	uint32_t height, 
	size_t resourceIndex,
	const glm::mat4 &reprojectionMatrix)
	:m_renderResources(renderResources),
	m_width(width),
	m_height(height),
	m_resourceIndex(resourceIndex),
	m_reprojectionMatrix(reprojectionMatrix)
{
	strcpy_s(m_pipelineDesc.m_computeShaderPath, "Resources/Shaders/velocityComposition_comp.spv");

	m_pipelineDesc.m_layout.m_setLayoutCount = 1;
	m_pipelineDesc.m_layout.m_setLayouts[0] = m_renderResources->m_descriptorSetLayouts[VELOCITY_COMPOSITION_SET_INDEX];
	m_pipelineDesc.m_layout.m_pushConstantRangeCount = 1;
	m_pipelineDesc.m_layout.m_pushConstantRanges[0] = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConsts) };
}

void VEngine::VKVelocityCompositionPass::addToGraph(FrameGraph::Graph &graph, FrameGraph::ImageHandle depthImageHandle, FrameGraph::ImageHandle velocityImageHandle)
{
	auto builder = graph.addComputePass("Velocity Composition Pass", this, &m_pipelineDesc, FrameGraph::QueueType::GRAPHICS);

	// velocity composition set
	builder.readTexture(depthImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_pointSamplerClamp, m_renderResources->m_descriptorSets[m_resourceIndex][VELOCITY_COMPOSITION_SET_INDEX], VelocityCompositionSetBindings::DEPTH_IMAGE_BINDING);
	builder.readWriteStorageImage(velocityImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_renderResources->m_descriptorSets[m_resourceIndex][VELOCITY_COMPOSITION_SET_INDEX], VelocityCompositionSetBindings::VELOCITY_IMAGE_BINDING);
}

void VEngine::VKVelocityCompositionPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, VkPipelineLayout layout, VkPipeline pipeline)
{
	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

	vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &m_renderResources->m_descriptorSets[m_resourceIndex][VELOCITY_COMPOSITION_SET_INDEX], 0, nullptr);

	PushConsts pushConsts;
	pushConsts.reprojectionMatrix = m_reprojectionMatrix;
	pushConsts.texelSize = 1.0f / glm::vec2(m_width, m_height);

	vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

	VKUtility::dispatchComputeHelper(cmdBuf, m_width, m_height, 1, 8, 8, 1);
}
