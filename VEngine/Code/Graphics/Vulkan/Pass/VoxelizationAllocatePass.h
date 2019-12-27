#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VoxelizationAllocatePass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_brickPointerImageHandle;
			ImageViewHandle m_markImageHandle;
			BufferViewHandle m_freeBricksBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}