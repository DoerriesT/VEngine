#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ReadBackCopyPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			gal::BufferCopy m_bufferCopy;
			rg::BufferViewHandle m_srcBuffer;
			gal::Buffer *m_dstBuffer;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}