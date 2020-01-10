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
			bool m_showAge;
			ImageViewHandle m_irradianceVolumeImageHandle;
			ImageViewHandle m_irradianceVolumeAgeImageHandle;
			ImageViewHandle m_colorImageHandle;
			ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}