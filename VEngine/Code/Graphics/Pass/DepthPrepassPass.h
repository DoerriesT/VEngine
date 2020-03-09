#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace DepthPrepassPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_alphaMasked;
			gal::DescriptorBufferInfo m_instanceDataBufferInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			gal::DescriptorBufferInfo m_subMeshInfoBufferInfo;
			rg::BufferViewHandle m_indicesBufferHandle;
			rg::BufferViewHandle m_indirectBufferHandle;
			rg::ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}