#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace FillLightingQueuesPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_ageImageHandle;
			ImageViewHandle m_hizImageHandle;
			BufferViewHandle m_queueBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
			VkDescriptorBufferInfo m_culledBufferInfo;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}