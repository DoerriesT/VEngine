#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VolumetricFogFilterPass2
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_ignoreHistory;
			float m_frustumCorners[4][3];
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
			rg::ImageViewHandle m_inputImageViewHandle;
			rg::ImageViewHandle m_historyImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}