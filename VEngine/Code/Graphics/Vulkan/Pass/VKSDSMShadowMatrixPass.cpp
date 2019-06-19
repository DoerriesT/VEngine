#include "VKSDSMShadowMatrixPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/sdsmShadowMatrix_bindings.h"
}


void VEngine::VKSDSMShadowMatrixPass::addToGraph(FrameGraph::Graph & graph, const Data & data)
{
	graph.addComputePass("SDSM Shadow Matrix Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readStorageBuffer(data.m_partitionBoundsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		builder.writeStorageBuffer(data.m_shadowDataBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/sdsmShadowMatrix_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// partition bounds buffer
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_partitionBoundsBufferHandle), PARTITION_BOUNDS_BINDING);

			// shadow data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_shadowDataBufferHandle), SHADOW_DATA_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		PushConsts pushConsts;
		pushConsts.cameraViewToLightView = data.m_cameraViewToLightView;
		pushConsts.lightView = data.m_lightView;

		assert(pushConsts.lightView[3][1] == 0.0f);
		assert(pushConsts.lightView[3][2] == 0.0f);

		pushConsts.lightView[3][1] = data.m_lightSpaceNear;
		pushConsts.lightView[3][2] = data.m_lightSpaceFar;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		vkCmdDispatch(cmdBuf, 1, 1, 1);
	});
}
