#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace RayTraceTestPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			VkDescriptorBufferInfo m_internalNodesBufferInfo;
			VkDescriptorBufferInfo m_leafNodesBufferInfo;
			ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}