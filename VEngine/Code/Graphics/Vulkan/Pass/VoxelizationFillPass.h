#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInfo;
	struct SubMeshInstanceData;

	namespace VoxelizationFillPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			bool m_lighting;
			uint32_t m_instanceDataCount;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshInfoBufferInfo;
			ImageViewHandle m_brickPointerImageHandle;
			BufferViewHandle m_binVisBricksBufferHandle;
			// everything below is ignored if m_lighting is true
			BufferViewHandle m_colorBricksBufferHandle;
			ImageViewHandle m_shadowImageViewHandle;
			VkDescriptorBufferInfo m_lightDataBufferInfo;
			VkDescriptorBufferInfo m_shadowMatricesBufferInfo;
			ImageViewHandle m_irradianceVolumeImageHandle;
			ImageViewHandle m_irradianceVolumeDepthImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}