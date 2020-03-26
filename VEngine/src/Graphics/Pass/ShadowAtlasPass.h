#pragma once
#include "Graphics/RenderGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInfo;
	struct SubMeshInstanceData;
	struct ShadowDrawInfo;

	struct ShadowDrawInfo
	{
		uint32_t m_shadowMatrixIdx;
		uint32_t m_drawListIdx;
		uint32_t m_offsetX;
		uint32_t m_offsetY;
		uint32_t m_size;
	};

	namespace ShadowAtlasPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_shadowMapSize;
			uint32_t m_drawInfoCount;
			const ShadowDrawInfo *m_shadowDrawInfo;
			const uint32_t *m_opaqueInstanceDataCounts;
			const uint32_t *m_opaqueInstanceDataOffsets;
			const uint32_t *m_maskedInstanceDataCounts;
			const uint32_t *m_maskedInstanceDataOffsets;
			const glm::mat4 *m_shadowMatrices;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			rg::ImageViewHandle m_shadowAtlasImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}