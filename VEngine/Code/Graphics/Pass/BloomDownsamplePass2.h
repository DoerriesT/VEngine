#pragma once
#include "Graphics/RenderGraph2.h"
#include "Graphics/Vulkan/Module/BloomModule.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace BloomDownsamplePass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			rg::ImageViewHandle m_inputImageViewHandle;
			rg::ImageViewHandle m_resultImageViewHandles[BloomModule::BLOOM_MIP_COUNT];
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}