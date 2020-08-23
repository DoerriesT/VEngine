#include "BuildIndexBufferPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

namespace
{
	using namespace glm;
#include "../../../../Application/Resources/Shaders/buildIndexBuffer_bindings.h"
}

void VEngine::BuildIndexBufferPass::addToGraph(rg::RenderGraph &graph, const Data &data)
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
	DescriptorBufferInfo clusterInfoBufferInfo{ nullptr, 0, glm::max(clusterInfoList.size() * sizeof(ClusterInfo), size_t(1)) };
	{
		uint8_t *bufferPtr;
		data.m_passRecordContext->m_renderResources->m_mappableSSBOBlock[data.m_passRecordContext->m_commonRenderData->m_curResIdx]->allocate(clusterInfoBufferInfo.m_range, clusterInfoBufferInfo.m_offset, clusterInfoBufferInfo.m_buffer, bufferPtr);
		if (!clusterInfoList.empty())
		{
			memcpy(bufferPtr, clusterInfoList.data(), clusterInfoList.size() * sizeof(ClusterInfo));
		}
	}

	// indirect draw buffer
	{
		rg::BufferDescription bufferDesc{};
		bufferDesc.m_name = "Indirect Draw Buffer";
		bufferDesc.m_size = glm::max(32ull, sizeof(DrawIndexedIndirectCommand));

		*data.m_indirectDrawCmdBufferViewHandle = graph.createBufferView({ bufferDesc.m_name, graph.createBuffer(bufferDesc), 0, bufferDesc.m_size });
	}

	// filtered indices buffer
	{
		rg::BufferDescription bufferDesc{};
		bufferDesc.m_name = "Filtered Indices Buffer";
		bufferDesc.m_size = glm::max(32u, filteredTriangleIndexBufferSize * 4);

		*data.m_filteredIndicesBufferViewHandle = graph.createBufferView({ bufferDesc.m_name, graph.createBuffer(bufferDesc), 0, bufferDesc.m_size });
	}

	rg::BufferViewHandle indirectDrawCmdBufferViewHandle = *data.m_indirectDrawCmdBufferViewHandle;
	rg::BufferViewHandle filteredIndicesBufferViewHandle = *data.m_filteredIndicesBufferViewHandle;
	const uint32_t clusterListSize = static_cast<uint32_t>(clusterInfoList.size());

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(indirectDrawCmdBufferViewHandle), {gal::ResourceState::WRITE_BUFFER_TRANSFER, PipelineStageFlagBits::TRANSFER_BIT}, true, {gal::ResourceState::READ_WRITE_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
		{rg::ResourceViewHandle(filteredIndicesBufferViewHandle), {gal::ResourceState::WRITE_STORAGE_BUFFER, PipelineStageFlagBits::COMPUTE_SHADER_BIT}},
	};

	graph.addPass("Build Index Buffer", data.m_async ? rg::QueueType::COMPUTE : rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
		{
			// prime indirect buffer with data
			{
				DrawIndexedIndirectCommand cmd{};
				cmd.m_instanceCount = 1;
				cmdList->updateBuffer(registry.getBuffer(indirectDrawCmdBufferViewHandle), 0, sizeof(cmd), &cmd);

				Barrier barrier = Initializers::bufferBarrier(registry.getBuffer(indirectDrawCmdBufferViewHandle),
					PipelineStageFlagBits::TRANSFER_BIT, PipelineStageFlagBits::COMPUTE_SHADER_BIT,
					gal::ResourceState::WRITE_BUFFER_TRANSFER, gal::ResourceState::READ_WRITE_STORAGE_BUFFER);
				cmdList->barrier(1, &barrier);
			}

			// create pipeline description
			ComputePipelineCreateInfo pipelineCreateInfo;
			ComputePipelineBuilder builder(pipelineCreateInfo);
			builder.setComputeShader("Resources/Shaders/buildIndexBuffer_comp.spv");

			auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

			cmdList->bindPipeline(pipeline);

			// update descriptor sets
			{
				DescriptorSet *descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipeline->getDescriptorSetLayout(0));

				const auto &renderResources = *data.m_passRecordContext->m_renderResources;

				DescriptorBufferInfo inputIndicesBufferInfo{ renderResources.m_indexBuffer, 0, renderResources.m_indexBuffer->getDescription().m_size };
				DescriptorBufferInfo drawCmdBufferInfo = registry.getBufferInfo(indirectDrawCmdBufferViewHandle);
				DescriptorBufferInfo filteredIndicesBufferInfo = registry.getBufferInfo(filteredIndicesBufferViewHandle);
				DescriptorBufferInfo positionsBufferInfo = { renderResources.m_vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(glm::vec3) };

				DescriptorSetUpdate2 updates[] =
				{
					Initializers::rwStructuredBuffer(&clusterInfoBufferInfo, CLUSTER_INFO_BINDING),
					Initializers::rwStructuredBuffer(&inputIndicesBufferInfo, INPUT_INDICES_BINDING),
					Initializers::rwStructuredBuffer(&drawCmdBufferInfo, DRAW_COMMAND_BINDING),
					Initializers::rwStructuredBuffer(&filteredIndicesBufferInfo, FILTERED_INDICES_BINDING),
					Initializers::rwStructuredBuffer(&data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING),
					Initializers::rwStructuredBuffer(&positionsBufferInfo, POSITIONS_BINDING),
				};

				descriptorSet->update((uint32_t)std::size(updates), updates);

				cmdList->bindDescriptorSets(pipeline, 0, 1, &descriptorSet);
			}

			uint32_t currentOffset = 0;
			uint32_t clusterCount = clusterListSize;
			const uint32_t limit = 1024; // TODO g_context.m_properties.limits.maxComputeWorkGroupCount[0];

			while (clusterCount)
			{
				PushConsts pushConsts;
				pushConsts.viewProjection = data.m_viewProjectionMatrix;
				pushConsts.resolution = glm::vec2(data.m_width, data.m_height);
				pushConsts.clusterOffset = currentOffset;
				pushConsts.cullBackface = uint32_t(data.m_cullBackFace);

				cmdList->pushConstants(pipeline, ShaderStageFlagBits::COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				uint32_t dispatchedSize = glm::min(limit, clusterCount);

				cmdList->dispatch(dispatchedSize, 1, 1);
				
				currentOffset += dispatchedSize;
				clusterCount -= dispatchedSize;
			}
		});
}
