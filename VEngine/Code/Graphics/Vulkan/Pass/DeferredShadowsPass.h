#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace DeferredShadowsPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_resultImageViewHandle;
			ImageViewHandle m_depthImageViewHandle;
			ImageViewHandle m_shadowImageViewHandle;
			VkDescriptorBufferInfo m_lightDataBufferInfo;
			VkDescriptorBufferInfo m_shadowMatricesBufferInfo;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}