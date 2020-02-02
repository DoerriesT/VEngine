#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace ForwardLightingPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			VkDescriptorBufferInfo m_directionalLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightDataBufferInfo;
			VkDescriptorBufferInfo m_pointLightZBinsBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshInfoBufferInfo;
			BufferViewHandle m_pointLightBitMaskBufferHandle;
			BufferViewHandle m_indicesBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
			ImageViewHandle m_deferredShadowImageViewHandle;
			ImageViewHandle m_depthImageViewHandle;
			ImageViewHandle m_resultImageViewHandle;
			ImageViewHandle m_normalImageViewHandle;
			ImageViewHandle m_specularRoughnessImageViewHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}