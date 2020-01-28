#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IndirectLightPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_depthImageHandle;
			ImageViewHandle m_albedoImageHandle;
			ImageViewHandle m_normalImageHandle;
			ImageViewHandle m_indirectDiffuseImageHandle;
			//ImageViewHandle m_indirectSpecularImageHandle;
			ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}