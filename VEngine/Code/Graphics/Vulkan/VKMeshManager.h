#pragma once
#include "Handles.h"
#include "VKBuffer.h"
#include "Utility/TLSFAllocator.h"

namespace VEngine
{
	struct SubMeshData;
	struct SubMesh;

	class VKMeshManager
	{
	public:
		explicit VKMeshManager(VKBuffer &stagingBuffer, VKBuffer &vertexBuffer, VKBuffer &indexBuffer, VKBuffer &subMeshInfoBuffer);
		~VKMeshManager();
		void createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles);
		void destroySubMeshes(uint32_t count, SubMeshHandle *handles);

	private:
		TLSFAllocator m_vertexDataAllocator;
		TLSFAllocator m_indexDataAllocator;
		VKBuffer &m_stagingBuffer;
		VKBuffer &m_vertexBuffer;
		VKBuffer &m_indexBuffer;
		VKBuffer &m_subMeshInfoBuffer;
		SubMeshHandle *m_freeHandles;
		uint32_t m_freeHandleCount;
		uint32_t m_vertexCount;
		uint32_t m_indexCount;
		void **m_vertexSpans;
		void **m_indexSpans;
	};
}