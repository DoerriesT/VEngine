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
			ImageViewHandle m_brickPtrImageHandle;
			ImageViewHandle m_binVisBricksImageHandle;
			ImageViewHandle m_colorBricksImageHandle;
			ImageViewHandle m_rayMarchingResultImageHandle;
			ImageViewHandle m_rayMarchingResultDistanceImageHandle;
			BufferViewHandle m_queueBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}