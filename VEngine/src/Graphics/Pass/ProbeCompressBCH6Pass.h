#pragma once
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ProbeCompressBCH6Pass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_probeIndex;
			gal::ImageView *m_inputImageViews[RendererConsts::REFLECTION_PROBE_MIPS]; // REFLECTION_PROBE_MIPS mips with 6 layers each
			gal::ImageView *m_tmpResultImageViews[RendererConsts::REFLECTION_PROBE_MIPS]; // REFLECTION_PROBE_MIPS mips with 6 layers each
			gal::Image *m_compressedCubeArrayImage;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}