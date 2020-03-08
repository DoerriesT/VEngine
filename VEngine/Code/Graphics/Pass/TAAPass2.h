#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace TAAPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			float m_jitterOffsetX;
			float m_jitterOffsetY;
			rg::ImageViewHandle m_depthImageHandle;
			rg::ImageViewHandle m_velocityImageHandle;
			rg::ImageViewHandle m_taaHistoryImageHandle;
			rg::ImageViewHandle m_lightImageHandle;
			rg::ImageViewHandle m_taaResolveImageHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}