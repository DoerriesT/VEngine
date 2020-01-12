#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace BloomModule
	{
		static constexpr uint32_t BLOOM_MIP_COUNT = 6;

		struct InputData
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_colorImageViewHandle;
		};

		struct OutputData
		{
			ImageViewHandle m_bloomImageViewHandle;
		};

		void addToGraph(RenderGraph &graph, const InputData &inData, OutputData &outData);
	};
}