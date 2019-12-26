#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace IrradianceVolumeRayMarching2Pass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_brickPtrImageHandle;
			BufferViewHandle m_binVisBricksBufferHandle;
			BufferViewHandle m_colorBricksBufferHandle;
			ImageViewHandle m_rayMarchingResultImageHandle;
			ImageViewHandle m_rayMarchingResultDistanceImageHandle;
			BufferViewHandle m_queueBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}