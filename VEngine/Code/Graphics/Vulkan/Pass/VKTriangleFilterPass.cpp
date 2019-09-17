//#include "VKTriangleFilterPass.h"
//#include "Graphics/Vulkan/VKContext.h"
//#include "Graphics/Vulkan/VKRenderResources.h"
//#include "Graphics/Vulkan/VKPipelineCache.h"
//#include "Graphics/Vulkan/VKDescriptorSetCache.h"
//#include <glm/vec2.hpp>
//
//namespace
//{
//	using namespace glm;
//#include "../../../../../Application/Resources/Shaders/triangleFilter_bindings.h"
//}
//
//void VEngine::VKTriangleFilterPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
//{
//	graph.addComputePass("Triangle Filter Pass", FrameGraph::QueueType::GRAPHICS,
//		[&](FrameGraph::PassBuilder builder)
//	{
//		builder.writeStorageBuffer(data.m_indexCountsBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
//		builder.writeStorageBuffer(data.m_filteredIndicesBufferHandle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
//	},
//		[=](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
//	{
//		// create pipeline description
//		VKComputePipelineDescription pipelineDesc;
//		{
//			strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/triangleFilter_comp.spv");
//
//			pipelineDesc.finalize();
//		}
//
//		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc);
//
//		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);
//
//		// update descriptor sets
//		{
//			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);
//
//			// cluster info
//			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_clusterInfoBufferInfo, CLUSTER_INFO_BINDING);
//
//			// input indices
//			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_inputIndicesBufferInfo, INPUT_INDICES_BINDING);
//
//			// index offsets
//			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_indexOffsetsBufferInfo, INDEX_OFFSETS_BINDING);
//
//			// index counts
//			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_indexCountsBufferHandle), INDEX_COUNTS_BINDING);
//
//			// filtered indices
//			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_filteredIndicesBufferHandle), FILTERED_INDICES_BINDING);
//
//			// transform data
//			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);
//
//			// positions
//			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_positionsBufferInfo, POSITIONS_BINDING);
//
//			// view projection matrix
//			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_viewProjectionMatrixBufferHandle), VIEW_PROJECTION_MATRIX_BINDING);
//
//			writer.commit();
//		}
//
//		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
//		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);
//
//		uint32_t currentOffset = data.m_clusterOffset;
//		uint32_t clusterCount = data.m_clusterCount;
//		const uint32_t limit = g_context.m_properties.limits.maxComputeWorkGroupCount[0];
//
//		while (clusterCount)
//		{
//			PushConsts pushConsts;
//			pushConsts.resolution = glm::vec2(data.m_width, data.m_height);
//			pushConsts.clusterOffset = currentOffset;
//			pushConsts.cullBackface = data.m_cullBackface;
//			pushConsts.viewProjectionMatrixOffset = data.m_matrixBufferOffset;
//
//			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);
//
//			uint32_t dispatchedSize = std::min(limit, clusterCount);
//
//			vkCmdDispatch(cmdBuf, dispatchedSize, 1, 1);
//
//			currentOffset += dispatchedSize;
//
//			clusterCount -= dispatchedSize;
//		}
//	});
//}
