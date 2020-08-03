#include "MeshManager.h"
#include "RendererConsts.h"
#include "Graphics/RenderData.h"
#include "Graphics/Mesh.h"
#include "Utility/Utility.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "gal/Initializers.h"

using namespace VEngine::gal;

VEngine::MeshManager::MeshManager(
	GraphicsDevice *graphicsDevice, 
	Buffer *stagingBuffer, 
	Buffer *vertexBuffer, 
	Buffer *indexBuffer,
	Buffer *subMeshInfoBuffer,
	Buffer *subMeshBoundingBoxesBuffer,
	gal::Buffer *subMeshTexCoordScaleBiasBuffer)
	:m_graphicsDevice(graphicsDevice),
	m_cmdListPool(),
	m_cmdList(), m_vertexDataAllocator(RendererConsts::MAX_VERTICES, 1),
	m_indexDataAllocator(RendererConsts::MAX_INDICES * sizeof(uint16_t), 1),
	m_stagingBuffer(stagingBuffer),
	m_vertexBuffer(vertexBuffer),
	m_indexBuffer(indexBuffer),
	m_subMeshInfoBuffer(subMeshInfoBuffer),
	m_subMeshBoundingBoxesBuffer(subMeshBoundingBoxesBuffer),
	m_subMeshTexCoordScaleBiasBuffer(subMeshTexCoordScaleBiasBuffer),
	m_freeHandles(new uint32_t[RendererConsts::MAX_SUB_MESHES]),
	m_freeHandleCount(RendererConsts::MAX_SUB_MESHES),
	m_vertexCount(),
	m_indexCount(),
	m_vertexSpans(new void *[RendererConsts::MAX_SUB_MESHES]),
	m_indexSpans(new void *[RendererConsts::MAX_SUB_MESHES]),
	m_subMeshInfo(new SubMeshInfo[RendererConsts::MAX_SUB_MESHES])
{
	m_graphicsDevice->createCommandListPool(m_graphicsDevice->getGraphicsQueue(), &m_cmdListPool);
	m_cmdListPool->allocate(1, &m_cmdList);

	for (uint32_t i = 0; i < RendererConsts::MAX_SUB_MESHES; ++i)
	{
		m_freeHandles[i] = RendererConsts::MAX_SUB_MESHES - i - 1;
	}
}

VEngine::MeshManager::~MeshManager()
{
	delete[] m_freeHandles;

	m_cmdListPool->free(1, &m_cmdList);
	m_graphicsDevice->destroyCommandListPool(m_cmdListPool);
}

void VEngine::MeshManager::createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles)
{
	BufferCopy *bufferCopies = new BufferCopy[count * 7];

	// map staging buffer
	uint8_t *stagingBufferPtr;
	m_stagingBuffer->map((void **)&stagingBufferPtr);

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
			handles[i] = { m_freeHandles[m_freeHandleCount] };
		}

		SubMeshInfo subMeshInfo;

		// vertex data
		{
			const uint32_t vertexCount = subMeshes[i].m_vertexCount;
			uint32_t vertexOffset;
			if (!m_vertexDataAllocator.alloc(vertexCount, 1, vertexOffset, m_vertexSpans[handles[i].m_handle]))
			{
				Utility::fatalExit("Failed to allocate space in vertex buffer!", EXIT_FAILURE);
			}

			subMeshInfo.m_vertexOffset = vertexOffset;
			assert(subMeshInfo.m_vertexOffset >= 0);

			// positions
			{
				auto &positionsCopy = bufferCopies[i + count * 0];
				positionsCopy.m_srcOffset = currentStagingBufferOffset;
				positionsCopy.m_dstOffset = vertexOffset * sizeof(VertexPosition);
				positionsCopy.m_size = vertexCount * sizeof(VertexPosition);

				// copy to staging buffer
				memcpy(stagingBufferPtr + currentStagingBufferOffset, subMeshes[i].m_positions, positionsCopy.m_size);

				currentStagingBufferOffset += positionsCopy.m_size;
				assert(positionsCopy.m_dstOffset + positionsCopy.m_size <= m_vertexBuffer->getDescription().m_size);
			}

			// qtangents
			{
				auto &qtangentsCopy = bufferCopies[i + count * 1];
				qtangentsCopy.m_srcOffset = currentStagingBufferOffset;
				qtangentsCopy.m_dstOffset = RendererConsts::MAX_VERTICES * sizeof(VertexPosition) + vertexOffset * sizeof(VertexQTangent);
				qtangentsCopy.m_size = vertexCount * sizeof(VertexQTangent);

				// copy to staging buffer
				memcpy(stagingBufferPtr + currentStagingBufferOffset, subMeshes[i].m_qtangents, qtangentsCopy.m_size);

				currentStagingBufferOffset += qtangentsCopy.m_size;
				assert(qtangentsCopy.m_dstOffset + qtangentsCopy.m_size <= m_vertexBuffer->getDescription().m_size);
			}

			// texcoords
			{
				auto &texCoordsCopy = bufferCopies[i + count * 2];
				texCoordsCopy.m_srcOffset = currentStagingBufferOffset;
				texCoordsCopy.m_dstOffset = RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexQTangent)) + vertexOffset * sizeof(VertexTexCoord);
				texCoordsCopy.m_size = vertexCount * sizeof(VertexTexCoord);

				// copy to staging buffer
				memcpy(stagingBufferPtr + currentStagingBufferOffset, subMeshes[i].m_texCoords, texCoordsCopy.m_size);

				currentStagingBufferOffset += texCoordsCopy.m_size;
				assert(texCoordsCopy.m_dstOffset + texCoordsCopy.m_size <= m_vertexBuffer->getDescription().m_size);
			}
		}

		// index data
		{
			const uint32_t indexCount = subMeshes[i].m_indexCount;

			uint32_t indexOffset;
			if (!m_indexDataAllocator.alloc(indexCount * sizeof(uint16_t), 1, indexOffset, m_indexSpans[handles[i].m_handle]))
			{
				Utility::fatalExit("Failed to allocate space in index buffer", EXIT_FAILURE);
			}

			assert(indexOffset % sizeof(uint16_t) == 0);
			subMeshInfo.m_indexCount = indexCount;
			subMeshInfo.m_firstIndex = indexOffset / sizeof(uint16_t);

			auto &indexCopy = bufferCopies[i + count * 3];
			indexCopy.m_srcOffset = currentStagingBufferOffset;
			indexCopy.m_dstOffset = indexOffset;
			indexCopy.m_size = indexCount * sizeof(uint16_t);

			// copy to staging buffer
			memcpy(stagingBufferPtr + currentStagingBufferOffset, subMeshes[i].m_indices, indexCopy.m_size);

			currentStagingBufferOffset += indexCopy.m_size;
			assert(indexCopy.m_dstOffset + indexCopy.m_size <= m_indexBuffer->getDescription().m_size);
		}

		// subMeshInfo
		{
			auto &subMeshDataCopy = bufferCopies[i + count * 4];
			subMeshDataCopy.m_srcOffset = currentStagingBufferOffset;
			subMeshDataCopy.m_dstOffset = handles[i].m_handle * sizeof(SubMeshInfo);
			subMeshDataCopy.m_size = sizeof(SubMeshInfo);

			// copy to staging buffer
			memcpy(stagingBufferPtr + currentStagingBufferOffset, &subMeshInfo, subMeshDataCopy.m_size);

			currentStagingBufferOffset += subMeshDataCopy.m_size;
			assert(subMeshDataCopy.m_dstOffset + subMeshDataCopy.m_size <= m_subMeshInfoBuffer->getDescription().m_size);

			m_subMeshInfo[handles[i].m_handle] = subMeshInfo;
		}

		// bounding box
		{
			float boundingBoxData[6];
			for (size_t j = 0; j < 6; ++j)
			{
				boundingBoxData[j] = j < 3 ? subMeshes[i].m_minCorner[j] : subMeshes[i].m_maxCorner[j - 3];
			}

			auto &boundingBoxCopy = bufferCopies[i + count * 5];
			boundingBoxCopy.m_srcOffset = currentStagingBufferOffset;
			boundingBoxCopy.m_dstOffset = handles[i].m_handle * sizeof(boundingBoxData);
			boundingBoxCopy.m_size = sizeof(boundingBoxData);

			// copy to staging buffer
			memcpy(stagingBufferPtr + currentStagingBufferOffset, boundingBoxData, boundingBoxCopy.m_size);

			currentStagingBufferOffset += boundingBoxCopy.m_size;
			assert(boundingBoxCopy.m_dstOffset + boundingBoxCopy.m_size <= m_subMeshBoundingBoxesBuffer->getDescription().m_size);
		}

		// texcoord scale/bias
		{
			glm::vec4 scaleBiasData = glm::vec4(subMeshes[i].m_maxTexCoord - subMeshes[i].m_minTexCoord, subMeshes[i].m_minTexCoord);

			auto &scaleBiasCopy = bufferCopies[i + count * 6];
			scaleBiasCopy.m_srcOffset = currentStagingBufferOffset;
			scaleBiasCopy.m_dstOffset = handles[i].m_handle * sizeof(scaleBiasData);
			scaleBiasCopy.m_size = sizeof(scaleBiasData);

			// copy to staging buffer
			memcpy(stagingBufferPtr + currentStagingBufferOffset, &scaleBiasData, scaleBiasCopy.m_size);

			currentStagingBufferOffset += scaleBiasCopy.m_size;
			assert(scaleBiasCopy.m_dstOffset + scaleBiasCopy.m_size <= m_subMeshTexCoordScaleBiasBuffer->getDescription().m_size);
		}

		m_vertexCount += subMeshes[i].m_vertexCount;
		m_indexCount += subMeshes[i].m_indexCount;
	}

	assert(m_vertexCount <= RendererConsts::MAX_VERTICES);
	assert(m_indexCount <= RendererConsts::MAX_INDICES);
	assert(currentStagingBufferOffset <= RendererConsts::STAGING_BUFFER_SIZE);

	// copy from staging buffer to device local memory
	m_cmdListPool->reset();
	m_cmdList->begin();
	{
		m_cmdList->copyBuffer(m_stagingBuffer, m_vertexBuffer, count * 3, bufferCopies);
		m_cmdList->copyBuffer(m_stagingBuffer, m_indexBuffer, count, bufferCopies + count * 3);
		m_cmdList->copyBuffer(m_stagingBuffer, m_subMeshInfoBuffer, count, bufferCopies + count * 4);
		m_cmdList->copyBuffer(m_stagingBuffer, m_subMeshBoundingBoxesBuffer, count, bufferCopies + count * 5);
		m_cmdList->copyBuffer(m_stagingBuffer, m_subMeshTexCoordScaleBiasBuffer, count, bufferCopies + count * 6);
	}
	m_cmdList->end();
	Initializers::submitSingleTimeCommands(m_graphicsDevice->getGraphicsQueue(), m_cmdList);

	// unmap staging buffer
	m_stagingBuffer->unmap();

	delete[] bufferCopies;
}

void VEngine::MeshManager::destroySubMeshes(uint32_t count, SubMeshHandle *handles)
{
	for (uint32_t i = 0; i < count; ++i)
	{
		assert(m_freeHandleCount < RendererConsts::MAX_SUB_MESHES);
		m_vertexDataAllocator.free(m_vertexSpans[handles[i].m_handle]);
		m_indexDataAllocator.free(m_indexSpans[handles[i].m_handle]);
		m_freeHandles[m_freeHandleCount] = handles[i].m_handle;
		++m_freeHandleCount;
	}
}

const VEngine::SubMeshInfo *VEngine::MeshManager::getSubMeshInfo() const
{
	return m_subMeshInfo;
}
