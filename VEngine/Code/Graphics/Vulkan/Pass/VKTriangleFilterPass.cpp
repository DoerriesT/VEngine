#include "VKTriangleFilterPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include <glm/vec2.hpp>

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/triangleFilter_bindings.h"
}

void VEngine::VKTriangleFilterPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addComputePass("Triangle Filter Pass", FrameGraph::QueueType::GRAPHICS,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.writeStorageBuffer(data.m_indexCountsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		builder.writeStorageBuffer(data.m_filteredIndicesBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	},
		[=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKComputePipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_computeShaderPath, "Resources/Shaders/triangleFilter_comp.spv");

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[8] = {};

			// cluster info
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = CLUSTER_INFO_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[0].pBufferInfo = &data.m_clusterInfoBufferInfo;

			// input indices
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = INPUT_INDICES_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[1].pBufferInfo = &data.m_inputIndicesBufferInfo;

			// index offsets
			descriptorWrites[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[2].dstSet = descriptorSet;
			descriptorWrites[2].dstBinding = INDEX_OFFSETS_BINDING;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorCount = 1;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[2].pBufferInfo = &data.m_indexOffsetsBufferInfo;

			// index counts
			VkDescriptorBufferInfo indexCountsBufferInfo = registry.getBufferInfo(data.m_indexCountsBufferHandle);
			descriptorWrites[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[3].dstSet = descriptorSet;
			descriptorWrites[3].dstBinding = INDEX_COUNTS_BINDING;
			descriptorWrites[3].dstArrayElement = 0;
			descriptorWrites[3].descriptorCount = 1;
			descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[3].pBufferInfo = &indexCountsBufferInfo;

			// filtered indices
			VkDescriptorBufferInfo filteredIndicesBufferInfo = registry.getBufferInfo(data.m_filteredIndicesBufferHandle);
			descriptorWrites[4] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[4].dstSet = descriptorSet;
			descriptorWrites[4].dstBinding = FILTERED_INDICES_BINDING;
			descriptorWrites[4].dstArrayElement = 0;
			descriptorWrites[4].descriptorCount = 1;
			descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[4].pBufferInfo = &filteredIndicesBufferInfo;

			// transform data
			descriptorWrites[5] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[5].dstSet = descriptorSet;
			descriptorWrites[5].dstBinding = TRANSFORM_DATA_BINDING;
			descriptorWrites[5].dstArrayElement = 0;
			descriptorWrites[5].descriptorCount = 1;
			descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[5].pBufferInfo = &data.m_transformDataBufferInfo;

			// positions
			descriptorWrites[6] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[6].dstSet = descriptorSet;
			descriptorWrites[6].dstBinding = POSITIONS_BINDING;
			descriptorWrites[6].dstArrayElement = 0;
			descriptorWrites[6].descriptorCount = 1;
			descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[6].pBufferInfo = &data.m_positionsBufferInfo;

			// view projection matrix
			VkDescriptorBufferInfo viewProjectionBufferInfo = registry.getBufferInfo(data.m_viewProjectionMatrixBufferHandle);
			descriptorWrites[7] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[7].dstSet = descriptorSet;
			descriptorWrites[7].dstBinding = VIEW_PROJECTION_MATRIX_BINDING;
			descriptorWrites[7].dstArrayElement = 0;
			descriptorWrites[7].descriptorCount = 1;
			descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[7].pBufferInfo = &viewProjectionBufferInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		uint32_t currentOffset = data.m_clusterOffset;
		uint32_t clusterCount = data.m_clusterCount;
		const uint32_t limit = g_context.m_properties.limits.maxComputeWorkGroupCount[0];

		while (clusterCount)
		{
			PushConsts pushConsts;
			pushConsts.resolution = glm::vec2(data.m_width, data.m_height);
			pushConsts.clusterOffset = currentOffset;
			pushConsts.cullBackface = data.m_cullBackface;
			pushConsts.viewProjectionMatrixOffset = data.m_matrixBufferOffset;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			uint32_t dispatchedSize = std::min(limit, clusterCount);

			vkCmdDispatch(cmdBuf, dispatchedSize, 1, 1);

			currentOffset += dispatchedSize;

			clusterCount -= dispatchedSize;
		}
	});
}
