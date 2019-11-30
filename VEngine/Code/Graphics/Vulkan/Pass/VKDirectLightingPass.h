#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKDirectLightingPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_ssao;
			VkDescriptorBufferInfo m_directionalLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightZBinsBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			ImageViewHandle m_irradianceVolumeImageHandle;
			BufferViewHandle m_pointLightBitMaskBufferHandle;
			ImageViewHandle m_depthImageHandle;
			ImageViewHandle m_uvImageHandle;
			ImageViewHandle m_ddxyLengthImageHandle;
			ImageViewHandle m_ddxyRotMaterialIdImageHandle;
			ImageViewHandle m_tangentSpaceImageHandle;
			ImageViewHandle m_deferredShadowImageViewHandle;
			ImageViewHandle m_occlusionImageHandle;
			ImageViewHandle m_resultImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}