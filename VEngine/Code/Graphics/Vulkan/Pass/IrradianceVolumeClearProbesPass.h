#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IrradianceVolumeClearProbesPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_irradianceVolumeAgeImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}