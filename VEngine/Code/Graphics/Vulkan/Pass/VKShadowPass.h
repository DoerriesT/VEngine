#pragma once
#include "Graphics/Vulkan/RenderGraph.h"
#include <glm/mat4x4.hpp>

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
			uint32_t m_drawOffset;
			uint32_t m_drawCount;
			uint32_t m_shadowMapSize;
			glm::mat4 m_shadowMatrix;
			bool m_alphaMasked;
			bool m_clear;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			BufferViewHandle m_indirectBufferHandle;
			ImageViewHandle m_shadowImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}