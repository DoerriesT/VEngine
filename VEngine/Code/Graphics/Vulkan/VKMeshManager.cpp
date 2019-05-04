#include "VKMeshManager.h"
#include "Graphics/RendererConsts.h"
#include "Graphics/RenderData.h"
#include "Graphics/Mesh.h"
#include "Utility/Utility.h"
#include "VKContext.h"
#include "VKUtility.h"

VEngine::VKMeshManager::VKMeshManager(VKBuffer &stagingBuffer, VKBuffer &vertexBuffer, VKBuffer &indexBuffer, VKBuffer &subMeshInfoBuffer)
	:m_vertexDataAllocator(RendererConsts::MAX_VERTICES, 1),
	m_indexDataAllocator(RendererConsts::MAX_INDICES * sizeof(uint32_t), 1),
	m_stagingBuffer(stagingBuffer),
	m_vertexBuffer(vertexBuffer),
	m_indexBuffer(indexBuffer),
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

void VEngine::VKMeshManager::createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles)
{
	VkBufferCopy *bufferCopies = new VkBufferCopy[count * 5];

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
			const uint32_t vertexCount = subMeshes[i].m_vertexCount;
			uint32_t vertexOffset;
			if (!m_vertexDataAllocator.alloc(vertexCount, 1, vertexOffset, m_vertexSpans[handles[i]]))
			{
				Utility::fatalExit("Failed to allocate space in vertex buffer!", EXIT_FAILURE);
			}

			subMeshData.m_vertexOffset = vertexOffset;
			assert(subMeshData.m_vertexOffset >= 0);

			// positions
			{
				VkBufferCopy &positionsCopy = bufferCopies[i + count * 0];
				positionsCopy.srcOffset = currentStagingBufferOffset;
				positionsCopy.dstOffset = vertexOffset * sizeof(VertexPosition);
				positionsCopy.size = vertexCount * sizeof(VertexPosition);

				// copy to staging buffer
				memcpy(stagingBufferPtr + currentStagingBufferOffset, subMeshes[i].m_positions, positionsCopy.size);

				currentStagingBufferOffset += positionsCopy.size;
				assert(positionsCopy.dstOffset + positionsCopy.size <= m_vertexBuffer.getSize());
			}
			
			// normals
			{
				VkBufferCopy &normalsCopy = bufferCopies[i + count * 1];
				normalsCopy.srcOffset = currentStagingBufferOffset;
				normalsCopy.dstOffset = RendererConsts::MAX_VERTICES * sizeof(VertexPosition) + vertexOffset * sizeof(VertexNormal);
				normalsCopy.size = vertexCount * sizeof(VertexNormal);

				// copy to staging buffer
				memcpy(stagingBufferPtr + currentStagingBufferOffset, subMeshes[i].m_normals, normalsCopy.size);

				currentStagingBufferOffset += normalsCopy.size;
				assert(normalsCopy.dstOffset + normalsCopy.size <= m_vertexBuffer.getSize());
			}

			// texcoords
			{
				VkBufferCopy &texCoordsCopy = bufferCopies[i + count * 2];
				texCoordsCopy.srcOffset = currentStagingBufferOffset;
				texCoordsCopy.dstOffset = RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)) + vertexOffset * sizeof(VertexTexCoord);
				texCoordsCopy.size = vertexCount * sizeof(VertexTexCoord);

				// copy to staging buffer
				memcpy(stagingBufferPtr + currentStagingBufferOffset, subMeshes[i].m_texCoords, texCoordsCopy.size);

				currentStagingBufferOffset += texCoordsCopy.size;
				assert(texCoordsCopy.dstOffset + texCoordsCopy.size <= m_vertexBuffer.getSize());
			}
		}
		
		// index data
		{
			const uint32_t indexCount = subMeshes[i].m_indexCount;
			uint32_t indexOffset;
			if (!m_indexDataAllocator.alloc(indexCount * sizeof(uint32_t), 1, indexOffset, m_indexSpans[handles[i]]))
			{
				Utility::fatalExit("Failed to allocate space in index buffer", EXIT_FAILURE);
			}

			assert(indexOffset % sizeof(uint32_t) == 0);
			subMeshData.m_indexCount = indexCount;
			subMeshData.m_firstIndex = indexOffset / sizeof(uint32_t);

			VkBufferCopy &indexCopy = bufferCopies[i + count * 3];
			indexCopy.srcOffset = currentStagingBufferOffset;
			indexCopy.dstOffset = indexOffset;
			indexCopy.size = indexCount * sizeof(uint32_t);

			// copy to staging buffer
			memcpy(stagingBufferPtr + currentStagingBufferOffset, subMeshes[i].m_indices, indexCopy.size);

			currentStagingBufferOffset += indexCopy.size;
			assert(indexCopy.dstOffset + indexCopy.size <= m_indexBuffer.getSize());
		}

		// subMeshData
		{
			VkBufferCopy &subMeshDataCopy = bufferCopies[i + count * 4];
			subMeshDataCopy.srcOffset = currentStagingBufferOffset;
			subMeshDataCopy.dstOffset = handles[i] * sizeof(SubMeshData);
			subMeshDataCopy.size = sizeof(SubMeshData);

			// copy to staging buffer
			memcpy(stagingBufferPtr + currentStagingBufferOffset, &subMeshData, subMeshDataCopy.size);

			currentStagingBufferOffset += subMeshDataCopy.size;
			assert(subMeshDataCopy.dstOffset + subMeshDataCopy.size <= m_subMeshInfoBuffer.getSize());
		}

		m_vertexCount += subMeshes[i].m_vertexCount;
		m_indexCount += subMeshes[i].m_indexCount;
	}

	assert(m_vertexCount <= RendererConsts::MAX_VERTICES);
	assert(m_indexCount <= RendererConsts::MAX_INDICES);
	assert(currentStagingBufferOffset <= RendererConsts::STAGING_BUFFER_SIZE);

	// copy from staging buffer to device local memory
	VkCommandBuffer copyCmd = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
	{
		vkCmdCopyBuffer(copyCmd, m_stagingBuffer.getBuffer(), m_vertexBuffer.getBuffer(), count * 3, bufferCopies);
		vkCmdCopyBuffer(copyCmd, m_stagingBuffer.getBuffer(), m_indexBuffer.getBuffer(), count, bufferCopies + count * 3);
		vkCmdCopyBuffer(copyCmd, m_stagingBuffer.getBuffer(), m_subMeshInfoBuffer.getBuffer(), count, bufferCopies + count * 4);
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
