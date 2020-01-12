#pragma once
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Vulkan/Module/BloomModule.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace BloomDownsamplePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_inputImageViewHandle;
			ImageViewHandle m_resultImageViewHandles[BloomModule::BLOOM_MIP_COUNT];
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}