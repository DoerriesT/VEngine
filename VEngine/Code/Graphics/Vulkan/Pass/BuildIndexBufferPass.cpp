#include "BuildIndexBufferPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/buildIndexBuffer_bindings.h"
}

void VEngine::BuildIndexBufferPass::addToGraph(RenderGraph &graph, const Data &data)
{
	struct ClusterInfo
	{
		uint32_t indexCount;
		uint32_t indexOffset;
		int32_t vertexOffset;
		uint32_t drawCallIndex;
		uint32_t transformIndex;
	};

	std::vector<ClusterInfo> clusterInfoList;
	uint32_t filteredTriangleIndexBufferSize = 0;
	{
		for (uint32_t i = 0; i < data.m_instanceCount; ++i)
		{
			const uint32_t instanceIndex = i + data.m_instanceOffset;
			const auto &subMeshInfo = data.m_subMeshInfo[data.m_instanceData[instanceIndex].m_subMeshIndex];
			uint32_t indexCount = subMeshInfo.m_indexCount;
			uint32_t indexOffset = subMeshInfo.m_firstIndex;

			filteredTriangleIndexBufferSize += indexCount;

			while (indexCount)
			{
				ClusterInfo clusterInfo;
				clusterInfo.indexCount = glm::min(indexCount, RendererConsts::TRIANGLE_FILTERING_CLUSTER_SIZE * 3u); // clusterSize is in triangles, so one triangle -> three indices
				clusterInfo.indexOffset = indexOffset;
				clusterInfo.vertexOffset = subMeshInfo.m_vertexOffset;
				clusterInfo.drawCallIndex = instanceIndex;
				clusterInfo.transformIndex = data.m_instanceData[instanceIndex].m_transformIndex;

				clusterInfoList.push_back(clusterInfo);

				indexCount -= clusterInfo.indexCount;
				indexOffset += clusterInfo.indexCount;
			}
		}
	}

	// cluster info write
	VkDescriptorBufferInfo clusterInfoBufferInfo{ VK_NULL_HANDLE, 0, glm::max(clusterInfoList.size() * sizeof(ClusterInfo), size_t(1)) };
	{
		uint8_t *bufferPtr;
		data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[data.m_passRecordContext->m_commonRenderData->m_curResIdx]->allocate(clusterInfoBufferInfo.range, clusterInfoBufferInfo.offset, clusterInfoBufferInfo.buffer, bufferPtr);
		if (!clusterInfoList.empty())
		{
			memcpy(bufferPtr, clusterInfoList.data(), clusterInfoList.size() * sizeof(ClusterInfo));
		}
	}

	// indirect draw buffer
	{
		BufferDescription bufferDesc{};
		bufferDesc.m_name = "Indirect Draw Buffer";
		bufferDesc.m_size = glm::max(32ull, sizeof(VkDrawIndexedIndirectCommand));
		bufferDesc.m_concurrent = true;

		*data.m_indirectDrawCmdBufferViewHandle = graph.createBufferView({ bufferDesc.m_name, graph.createBuffer(bufferDesc), 0, bufferDesc.m_size });
	}

	// filtered indices buffer
	{
		BufferDescription bufferDesc{};
		bufferDesc.m_name = "Filtered Indices Buffer";
		bufferDesc.m_size = glm::max(32u, filteredTriangleIndexBufferSize * 4);
		bufferDesc.m_concurrent = true;

		*data.m_filteredIndicesBufferViewHandle = graph.createBufferView({ bufferDesc.m_name, graph.createBuffer(bufferDesc), 0, bufferDesc.m_size });
	}

	BufferViewHandle indirectDrawCmdBufferViewHandle = *data.m_indirectDrawCmdBufferViewHandle;
	BufferViewHandle filteredIndicesBufferViewHandle = *data.m_filteredIndicesBufferViewHandle;
	const uint32_t clusterListSize = clusterInfoList.size();

	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(indirectDrawCmdBufferViewHandle), ResourceState::WRITE_BUFFER_TRANSFER, true, ResourceState::READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER},
		{ResourceViewHandle(filteredIndicesBufferViewHandle), ResourceState::WRITE_STORAGE_BUFFER_COMPUTE_SHADER},
	};

	graph.addPass("Build Index Buffer", data.m_async ? QueueType::COMPUTE : QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			// prime indirect buffer with data
			{
				VkDrawIndexedIndirectCommand cmd{};
				cmd.instanceCount = 1;
				vkCmdUpdateBuffer(cmdBuf, registry.getBuffer(indirectDrawCmdBufferViewHandle), 0, sizeof(cmd), &cmd);

				VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

				vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
			}

			// create pipeline description
			VKComputePipelineDescription pipelineDesc;
			{
				strcpy_s(pipelineDesc.m_computeShaderStage.m_path, "Resources/Shaders/buildIndexBuffer_comp.spv");
				pipelineDesc.finalize();
			}

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				const auto &renderResources = *data.m_passRecordContext->m_renderResources;

				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, clusterInfoBufferInfo, CLUSTER_INFO_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { renderResources.m_indexBuffer.getBuffer(), 0, renderResources.m_indexBuffer.getSize() }, INPUT_INDICES_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(indirectDrawCmdBufferViewHandle), DRAW_COMMAND_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(filteredIndicesBufferViewHandle), FILTERED_INDICES_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { renderResources.m_vertexBuffer.getBuffer(), 0, RendererConsts::MAX_VERTICES * sizeof(glm::vec3) }, POSITIONS_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_pipeline);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

			uint32_t currentOffset = 0;
			uint32_t clusterCount = clusterListSize;
			const uint32_t limit = g_context.m_properties.limits.maxComputeWorkGroupCount[0];

			const auto &renderData = *data.m_passRecordContext->m_commonRenderData;

			while (clusterCount)
			{
				PushConsts pushConsts;
				pushConsts.viewProjection = data.m_viewProjectionMatrix;
				pushConsts.resolution = glm::vec2(renderData.m_width, renderData.m_height);
				pushConsts.clusterOffset = currentOffset;
				pushConsts.cullBackface = 1;

				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				uint32_t dispatchedSize = glm::min(limit, clusterCount);

				vkCmdDispatch(cmdBuf, dispatchedSize, 1, 1);

				currentOffset += dispatchedSize;
				clusterCount -= dispatchedSize;
			}
		});
}
