#pragma once
#include "Graphics/RenderGraph.h"
#include "Graphics/Module/BloomModule.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace BloomDownsamplePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_inputImageViewHandle;
			rg::ImageViewHandle m_resultImageViewHandles[BloomModule::BLOOM_MIP_COUNT];
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}