#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IndirectDiffusePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_ssao;
			ImageViewHandle m_irradianceVolumeImageHandle;
			ImageViewHandle m_irradianceVolumeDepthImageHandle;
			ImageViewHandle m_depthImageHandle;
			ImageViewHandle m_albedoImageHandle;
			ImageViewHandle m_normalImageHandle;
			ImageViewHandle m_occlusionImageHandle;
			ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}