#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace DepthPrepassPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			bool m_alphaMasked;
			gal::DescriptorBufferInfo m_instanceDataBufferInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			gal::DescriptorBufferInfo m_subMeshInfoBufferInfo;
			rg::BufferViewHandle m_indicesBufferHandle;
			rg::BufferViewHandle m_indirectBufferHandle;
			rg::ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}