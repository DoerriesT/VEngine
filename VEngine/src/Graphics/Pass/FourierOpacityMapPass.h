#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct LocalParticipatingMedium;

	namespace FourierOpacityMapPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			//uint32_t m_localMediaCount;
			//LocalParticipatingMedium *m_localMedia;
			rg::ImageViewHandle m_fomImageViewHandle0;
			rg::ImageViewHandle m_fomImageViewHandle1;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}