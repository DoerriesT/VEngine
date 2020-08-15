#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct BillboardDrawData;

	namespace BillboardPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_billboardCount;
			const BillboardDrawData *m_billboards;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}