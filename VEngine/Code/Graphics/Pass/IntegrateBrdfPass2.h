#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace IntegrateBrdfPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			rg::ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}
