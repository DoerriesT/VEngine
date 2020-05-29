#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VolumetricFogExtinctionVolumePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			gal::DescriptorBufferInfo m_localMediaBufferInfo;
			gal::DescriptorBufferInfo m_globalMediaBufferInfo;
			rg::ImageViewHandle m_extinctionVolumeImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}