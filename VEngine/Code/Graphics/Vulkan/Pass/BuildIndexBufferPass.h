#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace BuildIndexBufferPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_clusterOffset;
			uint32_t m_clusterCount;
			VkDescriptorBufferInfo m_clusterInfoBufferInfo;
			VkDescriptorBufferInfo m_inputIndicesBufferInfo;
			VkDescriptorBufferInfo m_positionsBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			BufferViewHandle m_indirectDrawCmdBufferHandle;
			BufferViewHandle m_filteredIndicesBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}