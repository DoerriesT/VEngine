#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKPrepareIndirectBuffersPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_instanceCount;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshDataBufferInfo;
			BufferViewHandle m_indirectBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}