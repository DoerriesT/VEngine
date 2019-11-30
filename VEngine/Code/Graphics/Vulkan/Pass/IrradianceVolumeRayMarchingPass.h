#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IrradianceVolumeRayMarchingPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_voxelSceneOpacityImageHandle;
			ImageViewHandle m_voxelSceneImageHandle;
			ImageViewHandle m_rayMarchingResultImageHandle;
			BufferViewHandle m_queueBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}