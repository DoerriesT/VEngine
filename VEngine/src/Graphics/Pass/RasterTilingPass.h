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
			rg::BufferViewHandle m_punctualLightsBitMaskBufferHandle;
			rg::BufferViewHandle m_punctualLightsShadowedBitMaskBufferHandle;
			rg::BufferViewHandle m_participatingMediaBitMaskBufferHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}