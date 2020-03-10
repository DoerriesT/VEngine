#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInstanceData;
	struct SubMeshInfo;

	namespace DepthPrepassPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_alphaMasked;
			uint32_t m_instanceDataCount;
			uint32_t m_instanceDataOffset;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			//gal::DescriptorBufferInfo m_instanceDataBufferInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			//gal::DescriptorBufferInfo m_subMeshInfoBufferInfo;
			//rg::BufferViewHandle m_indicesBufferHandle;
			//rg::BufferViewHandle m_indirectBufferHandle;
			rg::ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}