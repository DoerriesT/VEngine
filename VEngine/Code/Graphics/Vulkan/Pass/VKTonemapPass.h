#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKTonemapPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_applyLinearToGamma;
			bool m_bloomEnabled;
			float m_bloomStrength;
			ImageViewHandle m_srcImageHandle;
			ImageViewHandle m_dstImageHandle;
			ImageViewHandle m_bloomImageViewHandle;
			BufferViewHandle m_avgLuminanceBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}