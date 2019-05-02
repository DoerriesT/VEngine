#pragma once
#include "Handles.h"
#include "VKBuffer.h"
#include "Utility/TLSFAllocator.h"

namespace VEngine
{
	struct SubMeshData;

	class VKMeshManager
	{
	public:
		explicit VKMeshManager(VKBuffer &stagingBuffer, VKBuffer &meshBuffer, VKBuffer &subMeshInfoBuffer);
		~VKMeshManager();
		void createSubMeshes(uint32_t count, uint32_t *vertexSizes, const uint8_t *const*vertexData, uint32_t *indexCounts, const uint32_t *const*indexData, SubMeshHandle *handles);
		void destroySubMeshes(uint32_t count, SubMeshHandle *handles);

	private:
		TLSFAllocator m_vertexDataAllocator;
		TLSFAllocator m_indexDataAllocator;
		VKBuffer &m_stagingBuffer;
		VKBuffer &m_meshBuffer;
		VKBuffer &m_subMeshInfoBuffer;
		SubMeshHandle *m_freeHandles;
		uint32_t m_freeHandleCount;
		void **m_vertexSpans;
		void **m_indexSpans;
	};
}