#pragma once
#include "Graphics/RenderGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInstanceData;
	struct SubMeshInfo;

	namespace ProbeGBufferPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			glm::mat4 m_viewProjectionMatrices[6];
			uint32_t m_opaqueInstanceDataCount[6];
			uint32_t m_opaqueInstanceDataOffset[6];
			uint32_t m_maskedInstanceDataCount[6];
			uint32_t m_maskedInstanceDataOffset[6];
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			gal::DescriptorBufferInfo m_materialDataBufferInfo;
			gal::DescriptorBufferInfo m_transformDataBufferInfo;
			gal::ImageView *m_depthImageViews[6];
			gal::ImageView *m_albedoRoughnessImageViews[6];
			gal::ImageView *m_normalImageViews[6];
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}