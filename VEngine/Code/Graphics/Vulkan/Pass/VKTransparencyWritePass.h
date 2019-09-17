//#pragma once
//#include "Graphics/Vulkan/RenderGraph.h"
//
//namespace VEngine
//{
//	struct PassRecordContext;
//
//	namespace VKTransparencyWritePass
//	{
//		struct Data
//		{
//			PassRecordContext *m_passRecordContext;
//			uint32_t m_drawCount;
//			VkDescriptorBufferInfo m_instanceDataBufferInfo;
//			VkDescriptorBufferInfo m_materialDataBufferInfo;
//			VkDescriptorBufferInfo m_transformDataBufferInfo;
//			VkDescriptorBufferInfo m_directionalLightDataBufferInfo;
//			VkDescriptorBufferInfo m_pointLightDataBufferInfo;
//			VkDescriptorBufferInfo m_shadowDataBufferInfo;
//			VkDescriptorBufferInfo m_pointLightZBinsBufferInfo;
//			BufferViewHandle m_shadowSplitsBufferHandle;
//			BufferViewHandle m_pointLightBitMaskBufferHandle;
//			BufferViewHandle m_indirectBufferHandle;
//			ImageViewHandle m_depthImageHandle;
//			ImageViewHandle m_shadowAtlasImageHandle;
//			ImageViewHandle m_lightImageHandle;
//		};
//
//		void addToGraph(FrameGraph::Graph &graph, const Data &data);
//	}
//}