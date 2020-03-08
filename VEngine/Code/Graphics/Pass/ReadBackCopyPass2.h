#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace ReadBackCopyPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			gal::BufferCopy m_bufferCopy;
			rg::BufferViewHandle m_srcBuffer;
			gal::Buffer *m_dstBuffer;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}