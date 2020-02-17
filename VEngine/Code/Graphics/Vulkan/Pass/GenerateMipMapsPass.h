#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace GenerateMipMapsPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_mipCount;
			ImageHandle m_imageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}