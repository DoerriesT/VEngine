#pragma once
#include "Utility/TLSFAllocator.h"
#include "Handles.h"
#include "gal/FwdDecl.h"

namespace VEngine
{
	struct SubMeshInfo;
	struct SubMesh;

	class MeshManager
	{
	public:
		explicit MeshManager(gal::GraphicsDevice *graphicsDevice, 
			gal::Buffer *stagingBuffer, 
			gal::Buffer *vertexBuffer, 
			gal::Buffer *indexBuffer, 
			gal::Buffer *subMeshInfoBuffer, 
			gal::Buffer *subMeshBoundingBoxesBuffer,
			gal::Buffer *subMeshTexCoordScaleBiasBuffer);
		~MeshManager();
		void createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles);
		void destroySubMeshes(uint32_t count, SubMeshHandle *handles);
		const SubMeshInfo *getSubMeshInfo() const;

	private:
		gal::GraphicsDevice *m_graphicsDevice;
		gal::CommandListPool *m_cmdListPool;
		gal::CommandList *m_cmdList;
		TLSFAllocator m_vertexDataAllocator;
		TLSFAllocator m_indexDataAllocator;
		gal::Buffer *m_stagingBuffer;
		gal::Buffer *m_vertexBuffer;
		gal::Buffer *m_indexBuffer;
		gal::Buffer *m_subMeshInfoBuffer;
		gal::Buffer *m_subMeshBoundingBoxesBuffer;
		gal::Buffer *m_subMeshTexCoordScaleBiasBuffer;
		uint32_t *m_freeHandles;
		uint32_t m_freeHandleCount;
		uint32_t m_vertexCount;
		uint32_t m_indexCount;
		void **m_vertexSpans;
		void **m_indexSpans;
		SubMeshInfo *m_subMeshInfo;
	};
}