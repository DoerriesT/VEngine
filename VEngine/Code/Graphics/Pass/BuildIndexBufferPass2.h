#pragma once
#include "Graphics/RenderGraph2.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct PassRecordContext2;
	struct SubMeshInfo;
	struct SubMeshInstanceData;

	namespace BuildIndexBufferPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			const SubMeshInfo *m_subMeshInfo;
			const SubMeshInstanceData *m_instanceData;
			bool m_async;
			bool m_cullBackFace;
			glm::mat4 m_viewProjectionMatrix;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_instanceOffset;
			uint32_t m_instanceCount;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			rg::BufferViewHandle *m_indirectDrawCmdBufferViewHandle;
			rg::BufferViewHandle *m_filteredIndicesBufferViewHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}