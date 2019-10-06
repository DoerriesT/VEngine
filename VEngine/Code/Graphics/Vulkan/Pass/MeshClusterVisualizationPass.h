#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace MeshClusterVisualizationPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_drawOffset;
			uint32_t m_drawCount;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			BufferViewHandle m_indirectBufferHandle;
			ImageViewHandle m_depthImageHandle;
			ImageViewHandle m_colorImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}