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
			rg::BufferViewHandle m_spotLightBitMaskBufferHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}