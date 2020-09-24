#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace FXAAPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_applyDither;
			rg::ImageViewHandle m_inputImageHandle;
			rg::ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}