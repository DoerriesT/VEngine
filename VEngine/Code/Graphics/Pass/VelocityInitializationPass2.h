#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace VelocityInitializationPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_velocityImageHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}