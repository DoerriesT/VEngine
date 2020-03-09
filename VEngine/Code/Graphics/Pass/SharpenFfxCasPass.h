#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace SharpenFfxCasPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_gammaSpaceInput;
			float m_sharpness;
			rg::ImageViewHandle m_srcImageHandle;
			rg::ImageViewHandle m_dstImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}