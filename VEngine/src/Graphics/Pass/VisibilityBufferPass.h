#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInstanceData;
	struct SubMeshInfo;

	namespace VisibilityBufferPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_opaqueInstanceDataCount;
			uint32_t m_opaqueInstanceDataOffset;
			uint32_t m_maskedInstanceDataCount;
			uint32_t m_maskedInstanceDataOffset;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			const float *m_texCoordScaleBias;
			//gal::DescriptorBufferInfo m_instanceDataBufferInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			//gal::DescriptorBufferInfo m_subMeshInfoBufferInfo;
			//rg::BufferViewHandle m_indicesBufferHandle;
			//rg::BufferViewHandle m_indirectBufferHandle;
			rg::ImageViewHandle m_triangleImageHandle;
			rg::ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}