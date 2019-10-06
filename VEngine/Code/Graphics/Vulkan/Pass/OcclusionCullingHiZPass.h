#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace OcclusionCullingHiZPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_drawOffset;
			uint32_t m_drawCount;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			VkDescriptorBufferInfo m_aabbBufferInfo;
			BufferViewHandle m_visibilityBufferHandle;
			ImageViewHandle m_depthPyramidImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}