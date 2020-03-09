#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace BloomModule
	{
		static constexpr uint32_t BLOOM_MIP_COUNT = 6;

		struct InputData
		{
			PassRecordContext *m_passRecordContext;
			rg::ImageViewHandle m_colorImageViewHandle;
		};

		struct OutputData
		{
			rg::ImageViewHandle m_bloomImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const InputData &inData, OutputData &outData);
	};
}