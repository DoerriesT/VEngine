#include "VKTonemapPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/VKContext.h"
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/tonemap_bindings.h"

//VEngine::VKTonemapPass::VKTonemapPass(VKRenderResources *renderResources, 
//	uint32_t width, 
//	uint32_t height, 
//	size_t resourceIndex)
//	:m_renderResources(renderResources),
//	m_width(width),
//	m_height(height),
//	m_resourceIndex(resourceIndex)
//{
//	strcpy_s(m_pipelineDesc.m_computeShaderPath, "Resources/Shaders/tonemap_comp.spv");
//}
//
//void VEngine::VKTonemapPass::addToGraph(FrameGraph::Graph &graph, FrameGraph::ImageHandle srcImageHandle, FrameGraph::ImageHandle dstImageHandle, FrameGraph::BufferHandle avgLuminanceBufferHandle)
//{
//	auto builder = graph.addComputePass("Tonemap Pass", this, &m_pipelineDesc, FrameGraph::QueueType::GRAPHICS);
//
//	// common set
//	builder.readStorageBuffer(avgLuminanceBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
//
//	// tonemap set
//	builder.readTexture(srcImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
//	builder.writeStorageImage(dstImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
//}
//
//void VEngine::VKTonemapPass::record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry & registry, VkPipelineLayout layout, VkPipeline pipeline)
//{
//	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
//
//	uint32_t luminanceIndex = static_cast<uint32_t>(m_resourceIndex);
//	vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(luminanceIndex), &luminanceIndex);
//
//	VKUtility::dispatchComputeHelper(cmdBuf, m_width, m_height, 1, 8, 8, 1);
//}

void VEngine::VKTonemapPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addComputePass("Tonemap Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readStorageBuffer(data.m_avgLuminanceBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readTexture(data.m_srcImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		builder.writeStorageImage(data.m_dstImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/tonemap_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[3] = {};

			// result image
			VkDescriptorImageInfo resultImageInfo = registry.getImageInfo(data.m_dstImageHandle);
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = RESULT_IMAGE_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptorWrites[0].pImageInfo = &resultImageInfo;

			// source image
			VkDescriptorImageInfo sourceImageInfo = registry.getImageInfo(data.m_srcImageHandle);
			sourceImageInfo.sampler = data.m_renderResources->m_pointSamplerClamp;
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = SOURCE_IMAGE_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].pImageInfo = &sourceImageInfo;

			// avg luminance
			VkDescriptorBufferInfo avgLuminanceBufferInfo = registry.getBufferInfo(data.m_avgLuminanceBufferHandle);
			descriptorWrites[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[2].dstSet = descriptorSet;
			descriptorWrites[2].dstBinding = LUMINANCE_VALUES_BINDING;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorCount = 1;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[2].pBufferInfo = &avgLuminanceBufferInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		uint32_t luminanceIndex = static_cast<uint32_t>(data.m_resourceIndex);
		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(luminanceIndex), &luminanceIndex);

		VKUtility::dispatchComputeHelper(cmdBuf, data.m_width, data.m_height, 1, 8, 8, 1);
	});
}
