#include "VKTriangleFilterModule.h"
#include "Graphics/RenderData.h"
#include "Graphics/RendererConsts.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKResourceDefinitions.h"
#include "Graphics/Vulkan/Pass/VKTriangleFilterPass.h"
#include "Graphics/Vulkan/Pass/VKDrawCallCompactionPass.h"

void VEngine::VKTriangleFilterModule::addToGraph(FrameGraph::Graph &graph, const InputData &inData, OutputData &outData)
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
	std::vector<uint32_t> filteredTriangleIndexOffsetList;
	uint32_t filteredTriangleIndexBufferSize = 0;
	{
		uint32_t drawCallIndex = 0;

		const auto *subMeshInfoList = inData.m_subMeshInfoList;

		auto createClusters = [&](uint32_t subMeshIndex, uint32_t transformIndex)
		{
			const auto &subMeshInfo = subMeshInfoList[subMeshIndex];
			uint32_t indexCount = subMeshInfo.m_indexCount;
			uint32_t indexOffset = subMeshInfo.m_firstIndex;

			filteredTriangleIndexOffsetList.push_back(filteredTriangleIndexBufferSize);
			filteredTriangleIndexBufferSize += indexCount;

			while (indexCount)
			{
				ClusterInfo clusterInfo;
				clusterInfo.indexCount = std::min(indexCount, RendererConsts::TRIANGLE_FILTERING_CLUSTER_SIZE * 3u); // clusterSize is in triangles, so one triangle -> three indices
				clusterInfo.indexOffset = indexOffset;
				clusterInfo.vertexOffset = subMeshInfo.m_vertexOffset;
				clusterInfo.drawCallIndex = drawCallIndex;
				clusterInfo.transformIndex = transformIndex;

				clusterInfoList.push_back(clusterInfo);

				indexCount -= clusterInfo.indexCount;
				indexOffset += clusterInfo.indexCount;
			}

			++drawCallIndex;
		};

		for (uint32_t i = 0; i < inData.m_instanceCount; ++i)
		{
			createClusters(inData.m_instanceData[i].m_subMeshIndex, inData.m_instanceData[i].m_transformIndex);
		}
	}

	// cluster info write
	VkDescriptorBufferInfo clusterInfoBufferInfo{ VK_NULL_HANDLE, 0, std::max(clusterInfoList.size() * sizeof(ClusterInfo), size_t(1)) };
	{
		uint8_t *bufferPtr;
		inData.m_renderResources->m_mappableSSBOBlock[inData.m_currentResourceIndex]->allocate(clusterInfoBufferInfo.range, clusterInfoBufferInfo.offset, clusterInfoBufferInfo.buffer, bufferPtr);
		if (!clusterInfoList.empty())
		{
			memcpy(bufferPtr, clusterInfoList.data(), clusterInfoList.size() * sizeof(ClusterInfo));
		}
	}

	// triangle filter index offsets
	VkDescriptorBufferInfo triangleFilterIndexOffsetsBufferInfo{ VK_NULL_HANDLE, 0, std::max(filteredTriangleIndexOffsetList.size() * sizeof(uint32_t), size_t(1)) };
	{
		uint8_t *bufferPtr;
		inData.m_renderResources->m_mappableSSBOBlock[inData.m_currentResourceIndex]->allocate(triangleFilterIndexOffsetsBufferInfo.range, triangleFilterIndexOffsetsBufferInfo.offset, triangleFilterIndexOffsetsBufferInfo.buffer, bufferPtr);
		if (!filteredTriangleIndexOffsetList.empty())
		{
			memcpy(bufferPtr, filteredTriangleIndexOffsetList.data(), filteredTriangleIndexOffsetList.size() * sizeof(uint32_t));
		}
	}

	FrameGraph::BufferHandle triangleFilterIndexCountsBufferHandle = VKResourceDefinitions::createTriangleFilterIndexCountsBufferHandle(graph, filteredTriangleIndexOffsetList.size());
	outData.m_filteredIndicesBufferHandle = VKResourceDefinitions::createTriangleFilterFilteredIndexBufferHandle(graph, filteredTriangleIndexBufferSize);
	outData.m_drawCountsBufferHandle = VKResourceDefinitions::createIndirectDrawCountsBufferHandle(graph, 1);
	outData.m_indirectBufferHandle = VKResourceDefinitions::createIndirectBufferHandle(graph, inData.m_instanceCount);

	// scene triangle filter
	VKTriangleFilterPass::Data triangleFilterPassData;
	triangleFilterPassData.m_renderResources = inData.m_renderResources;
	triangleFilterPassData.m_pipelineCache = inData.m_pipelineCache;
	triangleFilterPassData.m_descriptorSetCache = inData.m_descriptorSetCache;
	triangleFilterPassData.m_width = inData.m_width;
	triangleFilterPassData.m_height = inData.m_height;
	triangleFilterPassData.m_clusterOffset = 0;
	triangleFilterPassData.m_clusterCount = clusterInfoList.size();
	triangleFilterPassData.m_matrixBufferOffset = inData.m_viewProjectionBufferOffset;
	triangleFilterPassData.m_cullBackface = inData.m_cullBackface;
	triangleFilterPassData.m_clusterInfoBufferInfo = clusterInfoBufferInfo;
	triangleFilterPassData.m_inputIndicesBufferInfo = { inData.m_renderResources->m_indexBuffer.getBuffer(), 0, inData.m_renderResources->m_indexBuffer.getSize() };
	triangleFilterPassData.m_transformDataBufferInfo = inData.m_transformDataBufferInfo;
	triangleFilterPassData.m_positionsBufferInfo = { inData.m_renderResources->m_vertexBuffer.getBuffer(), 0, RendererConsts::MAX_VERTICES * sizeof(glm::vec3) };
	triangleFilterPassData.m_indexOffsetsBufferInfo = triangleFilterIndexOffsetsBufferInfo;
	triangleFilterPassData.m_indexCountsBufferHandle = triangleFilterIndexCountsBufferHandle;
	triangleFilterPassData.m_filteredIndicesBufferHandle = outData.m_filteredIndicesBufferHandle;
	triangleFilterPassData.m_viewProjectionMatrixBufferHandle = inData.m_viewProjectionBufferHandle;

	if (!clusterInfoList.empty())
	{
		VKTriangleFilterPass::addToGraph(graph, triangleFilterPassData);
	}


	// scene draw call compaction
	VKDrawCallCompactionPass::Data drawCallCompactionPassData;
	drawCallCompactionPassData.m_renderResources = inData.m_renderResources;
	drawCallCompactionPassData.m_pipelineCache = inData.m_pipelineCache;
	drawCallCompactionPassData.m_descriptorSetCache = inData.m_descriptorSetCache;
	drawCallCompactionPassData.m_drawCallOffset = 0;
	drawCallCompactionPassData.m_drawCallCount = inData.m_instanceCount;
	drawCallCompactionPassData.m_instanceOffset = inData.m_instanceOffset;
	drawCallCompactionPassData.m_drawCountsOffset = 0;
	drawCallCompactionPassData.m_instanceDataBufferInfo = inData.m_instanceDataBufferInfo;
	drawCallCompactionPassData.m_subMeshDataBufferInfo = { inData.m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, inData.m_renderResources->m_subMeshDataInfoBuffer.getSize() };
	drawCallCompactionPassData.m_indexOffsetsBufferInfo = triangleFilterIndexOffsetsBufferInfo;
	drawCallCompactionPassData.m_indexCountsBufferHandle = triangleFilterIndexCountsBufferHandle;
	drawCallCompactionPassData.m_indirectBufferHandle = outData.m_indirectBufferHandle;
	drawCallCompactionPassData.m_drawCountsBufferHandle = outData.m_drawCountsBufferHandle;

	if (!clusterInfoList.empty())
	{
		VKDrawCallCompactionPass::addToGraph(graph, drawCallCompactionPassData);
	}
}
