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
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/prepareIndirectBuffers_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[7] = {};

			// instance data
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = INSTANCE_DATA_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[0].pBufferInfo = &data.m_instanceDataBufferInfo;

			// subMeshData
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = SUB_MESH_DATA_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[1].pBufferInfo = &data.m_subMeshDataBufferInfo;

			// opaque indirect buffer
			VkDescriptorBufferInfo opaqueIndirectBufferInfo = registry.getBufferInfo(data.m_opaqueIndirectBufferHandle);
			descriptorWrites[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[2].dstSet = descriptorSet;
			descriptorWrites[2].dstBinding = OPAQUE_INDIRECT_BUFFER_BINDING;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorCount = 1;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[2].pBufferInfo = &opaqueIndirectBufferInfo;

			// masked indirect buffer
			VkDescriptorBufferInfo maskedIndirectBufferInfo = registry.getBufferInfo(data.m_maskedIndirectBufferHandle);
			descriptorWrites[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[3].dstSet = descriptorSet;
			descriptorWrites[3].dstBinding = MASKED_INDIRECT_BUFFER_BINDING;
			descriptorWrites[3].dstArrayElement = 0;
			descriptorWrites[3].descriptorCount = 1;
			descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[3].pBufferInfo = &maskedIndirectBufferInfo;

			// transparent indirect buffer
			VkDescriptorBufferInfo transparentIndirectBufferInfo = registry.getBufferInfo(data.m_transparentIndirectBufferHandle);
			descriptorWrites[4] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[4].dstSet = descriptorSet;
			descriptorWrites[4].dstBinding = TRANSPARENT_INDIRECT_BUFFER_BINDING;
			descriptorWrites[4].dstArrayElement = 0;
			descriptorWrites[4].descriptorCount = 1;
			descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[4].pBufferInfo = &transparentIndirectBufferInfo;

			// opaque shadow indirect buffer
			VkDescriptorBufferInfo opaqueShadowIndirectBufferInfo = registry.getBufferInfo(data.m_opaqueShadowIndirectBufferHandle);
			descriptorWrites[5] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[5].dstSet = descriptorSet;
			descriptorWrites[5].dstBinding = OPAQUE_SHADOW_INDIRECT_BUFFER_BINDING;
			descriptorWrites[5].dstArrayElement = 0;
			descriptorWrites[5].descriptorCount = 1;
			descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[5].pBufferInfo = &opaqueShadowIndirectBufferInfo;

			// masked shadow indirect buffer
			VkDescriptorBufferInfo maskedShadowIndirectBufferInfo = registry.getBufferInfo(data.m_maskedShadowIndirectBufferHandle);
			descriptorWrites[6] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[6].dstSet = descriptorSet;
			descriptorWrites[6].dstBinding = MASKED_SHADOW_INDIRECT_BUFFER_BINDING;
			descriptorWrites[6].dstArrayElement = 0;
			descriptorWrites[6].descriptorCount = 1;
			descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[6].pBufferInfo = &maskedShadowIndirectBufferInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
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
