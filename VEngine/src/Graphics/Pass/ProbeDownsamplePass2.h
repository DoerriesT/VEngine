#pragma once
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ProbeDownsamplePass2
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_resultImageViewHandles[RendererConsts::REFLECTION_PROBE_MIPS]; // REFLECTION_PROBE_MIPS mips with 6 layers each (array view)
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}