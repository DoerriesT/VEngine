#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ProbeCompressBCH6Pass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_probeIndex;
			gal::ImageView *m_inputImageViews[5]; // 5 mips with 6 layers each
			gal::ImageView *m_tmpResultImageViews[5]; // 5 mips with 6 layers each
			gal::Image *m_compressedCubeArrayImage;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}