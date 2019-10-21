#pragma once
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ClearVoxelsPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_voxelImageHandles[RendererConsts::VOXEL_SCENE_CASCADES];
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}