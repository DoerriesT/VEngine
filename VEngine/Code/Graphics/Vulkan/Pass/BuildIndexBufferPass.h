#pragma once
#include "Graphics/Vulkan/RenderGraph.h"
#include <glm/mat4x4.hpp>

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
			bool m_async;
			glm::mat4 m_viewProjectionMatrix;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_instanceOffset;
			uint32_t m_instanceCount;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			BufferViewHandle *m_indirectDrawCmdBufferViewHandle;
			BufferViewHandle *m_filteredIndicesBufferViewHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}