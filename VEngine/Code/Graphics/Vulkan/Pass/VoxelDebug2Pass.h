#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VoxelDebug2Pass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_cascadeIndex;
			ImageViewHandle m_voxelSceneImageHandle;
			ImageViewHandle m_colorImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}