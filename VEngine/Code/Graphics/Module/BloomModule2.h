#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace BloomModule2
	{
		static constexpr uint32_t BLOOM_MIP_COUNT = 6;

		struct InputData
		{
			PassRecordContext2 *m_passRecordContext;
			rg::ImageViewHandle m_colorImageViewHandle;
		};

		struct OutputData
		{
			rg::ImageViewHandle m_bloomImageViewHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const InputData &inData, OutputData &outData);
	};
}