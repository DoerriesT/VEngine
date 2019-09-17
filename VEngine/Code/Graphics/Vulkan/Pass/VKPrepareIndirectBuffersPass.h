#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKPrepareIndirectBuffersPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_opaqueCount;
			uint32_t m_maskedCount;
			uint32_t m_transparentCount;
			uint32_t m_opaqueShadowCount;
			uint32_t m_maskedShadowCount;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshDataBufferInfo;
			BufferViewHandle m_opaqueIndirectBufferHandle;
			BufferViewHandle m_maskedIndirectBufferHandle;
			BufferViewHandle m_transparentIndirectBufferHandle;
			BufferViewHandle m_opaqueShadowIndirectBufferHandle;
			BufferViewHandle m_maskedShadowIndirectBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}