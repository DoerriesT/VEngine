#pragma once
#include "Graphics/RenderGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInfo;
	struct SubMeshInstanceData;

	namespace ShadowPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_shadowMapSize;
			glm::mat4 m_shadowMatrix;
			uint32_t m_opaqueInstanceDataCount;
			uint32_t m_opaqueInstanceDataOffset;
			uint32_t m_maskedInstanceDataCount;
			uint32_t m_maskedInstanceDataOffset;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			//VkDescriptorBufferInfo m_instanceDataBufferInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			//VkDescriptorBufferInfo m_subMeshInfoBufferInfo;
			//BufferViewHandle m_indicesBufferHandle;
			//BufferViewHandle m_indirectBufferHandle;
			rg::ImageViewHandle m_shadowImageHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}