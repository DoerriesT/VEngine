#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace TonemapPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			bool m_applyLinearToGamma;
			bool m_bloomEnabled;
			float m_bloomStrength;
			rg::ImageViewHandle m_srcImageHandle;
			rg::ImageViewHandle m_dstImageHandle;
			rg::ImageViewHandle m_bloomImageViewHandle;
			rg::BufferViewHandle m_avgLuminanceBufferHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}