#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace LightIrradianceVolumePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_ageImageHandle;
			ImageViewHandle m_voxelSceneImageHandle;
			ImageViewHandle m_irradianceVolumeImageHandles[3];
			BufferViewHandle m_queueBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}