#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IrradianceVolumeDebugPass
	{
		struct Data
		{
			enum Mode
			{
				OCTAHEDRON,
				AMBIENT_CUBE,
				PROBE_AGE
			};
			PassRecordContext *m_passRecordContext;
			uint32_t m_cascadeIndex;
			Mode m_mode;
			ImageViewHandle m_irradianceVolumeImageHandle;
			ImageViewHandle m_irradianceVolumeAgeImageHandle;
			ImageViewHandle m_irradianceVolumeImageHandles[3];
			ImageViewHandle m_colorImageHandle;
			ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}