#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct LightData;

	namespace RasterTilingPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			const LightData *m_lightData;
			rg::BufferViewHandle m_pointLightBitMaskBufferHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}