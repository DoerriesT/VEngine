#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IntegrateBrdfPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}
