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
			rg::ImageViewHandle m_punctualLightsBitMaskImageHandle;
			rg::ImageViewHandle m_punctualLightsShadowedBitMaskImageHandle;
			rg::ImageViewHandle m_participatingMediaBitMaskImageHandle;
			rg::ImageViewHandle m_reflectionProbeBitMaskImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}