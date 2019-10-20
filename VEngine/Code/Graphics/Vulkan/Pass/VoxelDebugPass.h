#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VoxelDebugPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_voxelImageHandle;
			ImageViewHandle m_colorImageHandle;
			ImageViewHandle m_depthImageHandle;
			BufferViewHandle m_indirectBufferHandle;
			BufferViewHandle m_voxelPositionsHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}