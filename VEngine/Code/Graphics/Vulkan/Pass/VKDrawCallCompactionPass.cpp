#include "VKDrawCallCompactionPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/VKUtility.h"

namespace
{
	using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/drawCallCompaction_bindings.h"
}

void VEngine::VKDrawCallCompactionPass::addToGraph(FrameGraph::Graph & graph, const Data & data)
{
	graph.addComputePass("Draw Call Compaction Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readStorageBuffer(data.m_indexCountsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.writeStorageBuffer(data.m_indirectBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.writeStorageBuffer(data.m_drawCountsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/drawCallCompaction_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[6] = {};

			// index offsets
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = INDEX_OFFSETS_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[0].pBufferInfo = &data.m_indexOffsetsBufferInfo;

			// index counts
			VkDescriptorBufferInfo indexCountsBufferInfo = registry.getBufferInfo(data.m_indexCountsBufferHandle);
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = INDEX_COUNTS_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[1].pBufferInfo = &indexCountsBufferInfo;

			// instance data
			descriptorWrites[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[2].dstSet = descriptorSet;
			descriptorWrites[2].dstBinding = INSTANCE_DATA_BINDING;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorCount = 1;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[2].pBufferInfo = &data.m_instanceDataBufferInfo;

			// subMeshData
			descriptorWrites[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[3].dstSet = descriptorSet;
			descriptorWrites[3].dstBinding = SUB_MESH_DATA_BINDING;
			descriptorWrites[3].dstArrayElement = 0;
			descriptorWrites[3].descriptorCount = 1;
			descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[3].pBufferInfo = &data.m_subMeshDataBufferInfo;

			// indirect buffer
			VkDescriptorBufferInfo indirectBufferInfo = registry.getBufferInfo(data.m_indirectBufferHandle);
			descriptorWrites[4] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[4].dstSet = descriptorSet;
			descriptorWrites[4].dstBinding = INDIRECT_BUFFER_BINDING;
			descriptorWrites[4].dstArrayElement = 0;
			descriptorWrites[4].descriptorCount = 1;
			descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[4].pBufferInfo = &indirectBufferInfo;

			// draw counts buffer
			VkDescriptorBufferInfo drawCountsBufferInfo = registry.getBufferInfo(data.m_drawCountsBufferHandle);
			descriptorWrites[5] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[5].dstSet = descriptorSet;
			descriptorWrites[5].dstBinding = DRAW_COUNTS_BINDING;
			descriptorWrites[5].dstArrayElement = 0;
			descriptorWrites[5].descriptorCount = 1;
			descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[5].pBufferInfo = &drawCountsBufferInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		uint32_t currentDrawCallOffset = data.m_drawCallOffset;
		uint32_t currentInstanceOffset = data.m_instanceOffset;
		uint32_t drawCallCount = data.m_drawCallCount;
		const uint32_t groupSize = 64;
		const uint32_t limit = g_context.m_properties.limits.maxComputeWorkGroupCount[0] * groupSize;

		while (drawCallCount)
		{
			uint32_t dispatchedSize = std::min(limit, drawCallCount);

			PushConsts pushConsts;
			pushConsts.drawCallOffset = currentDrawCallOffset;
			pushConsts.instanceOffset = currentInstanceOffset;
			pushConsts.batchIndex = data.m_drawCountsOffset;
			pushConsts.drawCallCount = dispatchedSize;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			VKUtility::dispatchComputeHelper(cmdBuf, dispatchedSize, 1, 1, groupSize, 1, 1);

			drawCallCount -= dispatchedSize;
			currentDrawCallOffset += dispatchedSize;
			currentInstanceOffset += dispatchedSize;
		}
	});
}
