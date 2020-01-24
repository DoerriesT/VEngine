#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLightData;

	namespace DeferredShadowsPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_lightDataCount;
			const DirectionalLightData *m_lightData;
			ImageHandle m_resultImageHandle;
			ImageViewHandle m_depthImageViewHandle;
			ImageViewHandle m_tangentSpaceImageViewHandle;
			ImageViewHandle m_shadowImageViewHandle;
			VkDescriptorBufferInfo m_shadowMatricesBufferInfo;
			VkDescriptorBufferInfo m_cascadeParamsBufferInfo;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}