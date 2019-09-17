#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKSDSMModule
	{
		struct InputData
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_depthImageHandle;
		};

		struct OutputData
		{
			BufferViewHandle m_partitionBoundsBufferHandle;
			BufferViewHandle m_splitsBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const InputData &inData, OutputData &outData);
	};
}