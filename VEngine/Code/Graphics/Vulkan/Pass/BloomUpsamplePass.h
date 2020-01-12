#pragma once
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/Vulkan/Module/BloomModule.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace BloomUpsamplePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_inputImageViewHandles[BloomModule::BLOOM_MIP_COUNT];
			ImageViewHandle m_resultImageViewHandles[BloomModule::BLOOM_MIP_COUNT];
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}