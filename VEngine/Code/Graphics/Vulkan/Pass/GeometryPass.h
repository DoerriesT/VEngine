#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace GeometryPass
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
			ImageViewHandle m_uvImageHandle;
			ImageViewHandle m_ddxyLengthImageHandle;
			ImageViewHandle m_ddxyRotMaterialIdImageHandle;
			ImageViewHandle m_tangentSpaceImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}