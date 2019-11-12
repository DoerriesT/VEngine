#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ClearVoxelsPass
	{
		struct Data
		{
			enum ClearMode { VOXEL_SCENE, IRRADIANCE_VOLUME };
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_clearImageHandle;
			ClearMode m_clearMode;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}