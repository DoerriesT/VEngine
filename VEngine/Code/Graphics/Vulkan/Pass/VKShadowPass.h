#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshData;
	struct SubMeshInstanceData;
	struct ShadowJob;

	namespace VKShadowPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_drawCount;
			uint32_t m_shadowJobCount;
			const ShadowJob *m_shadowJobs;
			bool m_alphaMasked;
			bool m_clear;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			BufferViewHandle m_shadowDataBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
			ImageViewHandle m_shadowAtlasImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}