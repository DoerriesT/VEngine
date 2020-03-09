#pragma once
#include "Graphics/RenderGraph2.h"
#include "Graphics/Module/BloomModule2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace BloomUpsamplePass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			rg::ImageViewHandle m_inputImageViewHandles[BloomModule2::BLOOM_MIP_COUNT];
			rg::ImageViewHandle m_resultImageViewHandles[BloomModule2::BLOOM_MIP_COUNT];
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}