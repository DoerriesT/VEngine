#pragma once
#include "Graphics/Vulkan/RenderGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshData;
	struct SubMeshInstanceData;
	struct ShadowJob;

	namespace ShadowPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_shadowMapSize;
			glm::mat4 m_shadowMatrix;
			bool m_alphaMasked;
			bool m_clear;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshInfoBufferInfo;
			BufferViewHandle m_indicesBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
			ImageViewHandle m_shadowImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}