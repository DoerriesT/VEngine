#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace DepthPrepassPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_alphaMasked;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshInfoBufferInfo;
			BufferViewHandle m_indicesBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
			ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}