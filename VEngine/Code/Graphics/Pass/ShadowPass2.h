#pragma once
#include "Graphics/RenderGraph2.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct PassRecordContext2;
	struct SubMeshInfo;
	struct SubMeshInstanceData;

	namespace ShadowPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			uint32_t m_shadowMapSize;
			glm::mat4 m_shadowMatrix;
			bool m_alphaMasked;
			bool m_clear;
			uint32_t m_instanceDataCount;
			uint32_t m_instanceDataOffset;
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

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}