#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct LocalParticipatingMedium;

	namespace FourierOpacityPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			gal::DescriptorBufferInfo m_localMediaBufferInfo;
			gal::DescriptorBufferInfo m_globalMediaBufferInfo;
			rg::ImageViewHandle m_fomImageViewHandle0;
			rg::ImageViewHandle m_fomImageViewHandle1;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}