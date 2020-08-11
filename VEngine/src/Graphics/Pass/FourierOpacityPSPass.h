#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct FOMAtlasDrawInfo;
	struct LightData;

	namespace FourierOpacityPSPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_drawCount;
			const FOMAtlasDrawInfo *m_drawInfo;
			const LightData *m_lightData;
			gal::DescriptorBufferInfo m_localMediaBufferInfo;
			gal::DescriptorBufferInfo m_globalMediaBufferInfo;
			rg::ImageViewHandle m_fom0ImageViewHandle;
			rg::ImageViewHandle m_fom1ImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}