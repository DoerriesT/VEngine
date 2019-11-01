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
			uint32_t m_cascadeIndex;
			ImageViewHandle m_voxelSceneImageHandle;
			ImageViewHandle m_colorImageHandle;
			ImageViewHandle m_depthImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}