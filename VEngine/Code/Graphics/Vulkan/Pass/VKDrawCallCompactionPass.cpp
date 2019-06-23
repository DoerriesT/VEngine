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
			strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/drawCallCompaction_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// index offsets
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_indexOffsetsBufferInfo, INDEX_OFFSETS_BINDING);

			// index counts
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_indexCountsBufferHandle), INDEX_COUNTS_BINDING);

			// instance data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING);

			// submesh info
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_subMeshDataBufferInfo, SUB_MESH_DATA_BINDING);

			// indirect buffer
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_indirectBufferHandle), INDIRECT_BUFFER_BINDING);

			// draw counts
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_drawCountsBufferHandle), DRAW_COUNTS_BINDING);

			writer.commit();
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
