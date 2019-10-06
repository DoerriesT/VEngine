#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace OcclusionCullingCreateDrawArgsPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_drawOffset;
			uint32_t m_drawCount;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshInfoBufferInfo;
			BufferViewHandle m_indirectBufferHandle;
			BufferViewHandle m_drawCountsBufferHandle;
			BufferViewHandle m_visibilityBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}