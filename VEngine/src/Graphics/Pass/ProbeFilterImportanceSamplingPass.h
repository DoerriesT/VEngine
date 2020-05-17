#pragma once
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ProbeFilterImportanceSamplingPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_inputImageViewHandle; // cube with REFLECTION_PROBE_MIPS mips
			gal::ImageView *m_resultImageViews[RendererConsts::REFLECTION_PROBE_MIPS]; // REFLECTION_PROBE_MIPS mips with 6 layers each
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}