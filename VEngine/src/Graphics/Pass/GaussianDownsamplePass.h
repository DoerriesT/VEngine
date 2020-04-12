#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace GaussianDownsamplePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_levels;
			gal::Format m_format;
			rg::ImageHandle m_resultImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}