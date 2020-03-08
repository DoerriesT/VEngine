#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;
	struct LightData;

	namespace RasterTilingPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			const LightData *m_lightData;
			rg::BufferViewHandle m_pointLightBitMaskBufferHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}