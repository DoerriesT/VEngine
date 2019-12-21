#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace SharpenFfxCasPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_gammaSpaceInput;
			float m_sharpness;
			ImageViewHandle m_srcImageHandle;
			ImageViewHandle m_dstImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}