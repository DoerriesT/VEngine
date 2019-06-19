#include "VKSDSMBoundsReducePass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/sdsmBoundsReduce_bindings.h"
}

void VEngine::VKSDSMBoundsReducePass::addToGraph(FrameGraph::Graph & graph, const Data & data)
{
	graph.addComputePass("SDSM Bounds Reduce Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readTexture(data.m_depthImageHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.readStorageBuffer(data.m_splitsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		builder.readWriteStorageBuffer(data.m_partitionBoundsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/sdsmBoundsReduce_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// depth image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_depthImageHandle, data.m_renderResources->m_pointSamplerClamp), DEPTH_IMAGE_BINDING);

			// partition bounds buffer
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_partitionBoundsBufferHandle), PARTITION_BOUNDS_BINDING);

			// splits buffer
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_splitsBufferHandle), SPLITS_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		PushConsts pushConsts;
		pushConsts.invProjection = data.m_invProjection;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);
		
		VKUtility::dispatchComputeHelper(cmdBuf, data.m_width / 4, data.m_height / 4, 1, 16, 16, 1);
	});
}
