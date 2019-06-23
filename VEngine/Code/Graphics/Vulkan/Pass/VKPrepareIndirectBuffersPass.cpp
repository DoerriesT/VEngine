#include "VKPrepareIndirectBuffersPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"

namespace
{
	using mat3x4 = glm::mat3x4;
	using vec4 = glm::vec4;
	using mat4 = glm::mat4;
	using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/prepareIndirectBuffers_bindings.h"
}

void VEngine::VKPrepareIndirectBuffersPass::addToGraph(FrameGraph::Graph & graph, const Data & data)
{
	graph.addComputePass("Prepare Indirect Buffers Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.writeStorageBuffer(data.m_opaqueIndirectBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.writeStorageBuffer(data.m_maskedIndirectBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.writeStorageBuffer(data.m_transparentIndirectBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.writeStorageBuffer(data.m_opaqueShadowIndirectBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.writeStorageBuffer(data.m_maskedShadowIndirectBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/prepareIndirectBuffers_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// instance data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING);

			// submesh info
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_subMeshDataBufferInfo, SUB_MESH_DATA_BINDING);

			// opaque indirect buffer
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_opaqueIndirectBufferHandle), OPAQUE_INDIRECT_BUFFER_BINDING);

			// masked indirect buffer
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_maskedIndirectBufferHandle), MASKED_INDIRECT_BUFFER_BINDING);

			// transparent indirect buffer
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_transparentIndirectBufferHandle), TRANSPARENT_INDIRECT_BUFFER_BINDING);

			// opaque shadow indirect buffer
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_opaqueShadowIndirectBufferHandle), OPAQUE_SHADOW_INDIRECT_BUFFER_BINDING);

			// masked shadow indirect buffer
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_maskedShadowIndirectBufferHandle), MASKED_SHADOW_INDIRECT_BUFFER_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		PushConsts pushConsts;
		pushConsts.opaqueCount = data.m_opaqueCount;
		pushConsts.maskedCount = data.m_maskedCount;
		pushConsts.transparentCount = data.m_transparentCount;
		pushConsts.opaqueShadowCount = data.m_opaqueShadowCount;
		pushConsts.maskedShadowCount = data.m_maskedShadowCount;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

		VKUtility::dispatchComputeHelper(cmdBuf, data.m_opaqueCount + data.m_maskedCount + data.m_transparentCount + data.m_opaqueShadowCount + data.m_maskedShadowCount, 1, 1, 64, 1, 1);
	});
}
