#pragma once
#include "Graphics/RenderGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInfo;
	struct SubMeshInstanceData;
	struct ShadowAtlasDrawInfo;
	struct ViewRenderList;

	namespace ShadowAtlasPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_shadowMapSize;
			uint32_t m_drawInfoCount;
			const ShadowAtlasDrawInfo *m_shadowAtlasDrawInfo;
			const ViewRenderList *m_renderLists;
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