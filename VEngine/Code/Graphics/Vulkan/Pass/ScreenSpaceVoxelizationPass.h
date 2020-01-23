#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ScreenSpaceVoxelizationPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			VkDescriptorBufferInfo m_directionalLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightZBinsBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			BufferViewHandle m_pointLightBitMaskBufferHandle;
			ImageViewHandle m_depthImageHandle;
			ImageViewHandle m_uvImageHandle;
			ImageViewHandle m_ddxyLengthImageHandle;
			ImageViewHandle m_ddxyRotMaterialIdImageHandle;
			ImageViewHandle m_tangentSpaceImageHandle;
			ImageViewHandle m_deferredShadowImageViewHandle;
			ImageViewHandle m_brickPointerImageHandle;
			ImageViewHandle m_colorBricksImageHandle;
			ImageViewHandle m_irradianceVolumeImageHandle;
			ImageViewHandle m_irradianceVolumeDepthImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}