#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct LightData;

	namespace VKRasterTilingPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			const LightData *m_lightData;
			BufferViewHandle m_pointLightBitMaskBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}