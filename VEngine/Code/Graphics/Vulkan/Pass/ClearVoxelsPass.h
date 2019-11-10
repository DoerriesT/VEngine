#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ClearVoxelsPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_voxelSceneImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}