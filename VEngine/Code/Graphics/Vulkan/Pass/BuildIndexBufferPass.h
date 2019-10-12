#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInfo;
	struct SubMeshInstanceData;

	namespace BuildIndexBufferPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			const SubMeshInfo *m_subMeshInfo;
			const SubMeshInstanceData *m_instanceData;
			uint32_t m_instanceOffset;
			uint32_t m_instanceCount;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			BufferViewHandle *m_indirectDrawCmdBufferViewHandle;
			BufferViewHandle *m_filteredIndicesBufferViewHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}