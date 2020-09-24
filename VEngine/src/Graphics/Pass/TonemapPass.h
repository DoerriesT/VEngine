#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace TonemapPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_applyLinearToGamma;
			bool m_applyDither;
			bool m_bloomEnabled;
			float m_bloomStrength;
			rg::ImageViewHandle m_srcImageHandle;
			rg::ImageViewHandle m_dstImageHandle;
			rg::ImageViewHandle m_bloomImageViewHandle;
			rg::BufferViewHandle m_avgLuminanceBufferHandle;
			rg::BufferViewHandle m_exposureDataBufferHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}