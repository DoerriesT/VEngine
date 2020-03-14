#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInfo;
	struct SubMeshInstanceData;

	namespace GeometryPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_alphaMasked;
			uint32_t m_instanceDataCount;
			uint32_t m_instanceDataOffset;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			//VkDescriptorBufferInfo m_instanceDataBufferInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			//VkDescriptorBufferInfo m_subMeshInfoBufferInfo;
			//BufferViewHandle m_indicesBufferHandle;
			//BufferViewHandle m_indirectBufferHandle;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_uvImageHandle;
			rg::ImageViewHandle m_ddxyLengthImageHandle;
			rg::ImageViewHandle m_ddxyRotMaterialIdImageHandle;
			rg::ImageViewHandle m_tangentSpaceImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}