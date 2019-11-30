#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IrradianceVolumeUpdateProbesPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_ageImageHandle;
			ImageViewHandle m_irradianceVolumeImageHandle;
			ImageViewHandle m_rayMarchingResultImageHandle;
			BufferViewHandle m_queueBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}