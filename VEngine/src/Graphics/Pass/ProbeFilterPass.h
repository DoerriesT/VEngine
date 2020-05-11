#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ProbeFilterPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_inputImageViewHandle; // cube with 7 mips
			rg::ImageViewHandle m_resultImageViewHandles[7]; // 7 mips with 6 layers each
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}