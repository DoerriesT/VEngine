#include "VKMeshManager.h"
#include "Graphics/RendererConsts.h"
#include "Graphics/RenderData.h"
#include "Graphics/Mesh.h"
#include "Utility/Utility.h"
#include "VKContext.h"
#include "VKUtility.h"

VEngine::VKMeshManager::VKMeshManager(VKBuffer &stagingBuffer, VKBuffer &meshBuffer, VKBuffer &subMeshInfoBuffer)
	:m_vertexDataAllocator(RendererConsts::VERTEX_BUFFER_SIZE, 1),
	m_indexDataAllocator(RendererConsts::INDEX_BUFFER_SIZE, 1),
	m_stagingBuffer(stagingBuffer),
	m_meshBuffer(meshBuffer),
	m_subMeshInfoBuffer(subMeshInfoBuffer),
	m_freeHandles(new SubMeshHandle[RendererConsts::MAX_SUB_MESHES]),
	m_freeHandleCount(RendererConsts::MAX_SUB_MESHES),
	m_vertexSpans(new void*[RendererConsts::MAX_SUB_MESHES]),
	m_indexSpans(new void*[RendererConsts::MAX_SUB_MESHES])
{
	for (SubMeshHandle i = 0; i < RendererConsts::MAX_SUB_MESHES; ++i)
	{
		m_freeHandles[i] = RendererConsts::MAX_SUB_MESHES - i - 1;
	}
}

VEngine::VKMeshManager::~VKMeshManager()
{
	delete[] m_freeHandles;
}

void VEngine::VKMeshManager::createSubMeshes(uint32_t count, uint32_t *vertexSizes, const uint8_t *const*vertexData, uint32_t *indexCounts, const uint32_t *const*indexData, SubMeshHandle *handles)
{
	VkBufferCopy *bufferCopies = new VkBufferCopy[count * 3];

	// map staging buffer
	uint8_t *stagingBufferPtr;
	g_context.m_allocator.mapMemory(m_stagingBuffer.getAllocation(), (void **)&stagingBufferPtr);

	// vertex and index data is copied interleaved into the stagingbuffer, so remember the current offset
	size_t currentStagingBufferOffset = 0;

	for (uint32_t i = 0; i < count; ++i)
	{
		// acquire handle
		{
			if (!m_freeHandleCount)
			{
				Utility::fatalExit("Out of SubMesh Handles!", EXIT_FAILURE);
			}

			--m_freeHandleCount;
			handles[i] = m_freeHandles[m_freeHandleCount];
		}

		SubMeshData subMeshData;

		// vertex data
		{
			uint32_t vertexOffset;
			m_vertexDataAllocator.alloc(vertexSizes[i], 1, vertexOffset, m_vertexSpans[handles[i]]);

			assert(vertexOffset % sizeof(Vertex) == 0);
			subMeshData.m_vertexOffset = vertexOffset / sizeof(Vertex);

			VkBufferCopy &vertexCopy = bufferCopies[i];
			vertexCopy.srcOffset = currentStagingBufferOffset;
			vertexCopy.dstOffset = vertexOffset;
			vertexCopy.size = vertexSizes[i];

			// copy to staging buffer
			memcpy(stagingBufferPtr + currentStagingBufferOffset, vertexData[i], vertexCopy.size);

			currentStagingBufferOffset += vertexCopy.size;
		}
		
		// index data
		{
			uint32_t indexOffset;
			m_indexDataAllocator.alloc(indexCounts[i] * sizeof(uint32_t), 1, indexOffset, m_indexSpans[handles[i]]);

			assert(indexOffset % sizeof(uint32_t) == 0);
			subMeshData.m_indexCount = indexCounts[i];
			subMeshData.m_firstIndex = indexOffset / sizeof(uint32_t);

			VkBufferCopy &indexCopy = bufferCopies[i + count];
			indexCopy.srcOffset = currentStagingBufferOffset;
			indexCopy.dstOffset = RendererConsts::VERTEX_BUFFER_SIZE + indexOffset;
			indexCopy.size = indexCounts[i] * sizeof(uint32_t);

			// copy to staging buffer
			memcpy(stagingBufferPtr + currentStagingBufferOffset, indexData[i], indexCopy.size);

			currentStagingBufferOffset += indexCopy.size;
		}

		// subMeshData
		{
			VkBufferCopy &subMeshDataCopy = bufferCopies[i + count * 2];
			subMeshDataCopy.srcOffset = currentStagingBufferOffset;
			subMeshDataCopy.dstOffset = handles[i] * sizeof(SubMeshData);
			subMeshDataCopy.size = sizeof(SubMeshData);

			// copy to staging buffer
			memcpy(stagingBufferPtr + currentStagingBufferOffset, &subMeshData, subMeshDataCopy.size);

			currentStagingBufferOffset += subMeshDataCopy.size;
		}
	}

	// copy from staging buffer to device local memory
	VkCommandBuffer copyCmd = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
	{
		vkCmdCopyBuffer(copyCmd, m_stagingBuffer.getBuffer(), m_meshBuffer.getBuffer(), count * 2, bufferCopies);
		vkCmdCopyBuffer(copyCmd, m_stagingBuffer.getBuffer(), m_subMeshInfoBuffer.getBuffer(), count, bufferCopies + count * 2);
	}
	VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, copyCmd);

	// unmap staging buffer
	g_context.m_allocator.unmapMemory(m_stagingBuffer.getAllocation());

	delete[] bufferCopies;
}

void VEngine::VKMeshManager::destroySubMeshes(uint32_t count, SubMeshHandle *handles)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		assert(m_freeHandleCount < RendererConsts::MAX_SUB_MESHES);
		m_vertexDataAllocator.free(m_vertexSpans[handles[i]]);
		m_indexDataAllocator.free(m_indexSpans[handles[i]]);
		m_freeHandles[m_freeHandleCount] = handles[i];
		++m_freeHandleCount;
	}
}
