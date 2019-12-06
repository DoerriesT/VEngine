#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IrradianceVolumeUpdateACProbesPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_ageImageHandle;
			ImageViewHandle m_irradianceVolumeImageHandles[3];
			ImageViewHandle m_rayMarchingResultImageHandle;
			BufferViewHandle m_queueBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}