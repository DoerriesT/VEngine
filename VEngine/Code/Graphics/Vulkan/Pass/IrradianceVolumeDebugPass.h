#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IrradianceVolumeDebugPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_cascadeIndex;
			ImageViewHandle m_irradianceVolumeImageHandles[3];
			ImageViewHandle m_colorImageHandle;
			ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}