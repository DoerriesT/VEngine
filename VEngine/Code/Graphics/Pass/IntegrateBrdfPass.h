#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IntegrateBrdfPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}
