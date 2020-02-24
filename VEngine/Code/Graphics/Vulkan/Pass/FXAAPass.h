#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace FXAAPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_inputImageHandle;
			ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}