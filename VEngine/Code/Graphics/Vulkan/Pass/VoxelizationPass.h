#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct SubMeshInfo;
	struct SubMeshInstanceData;

	namespace VoxelizationPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_instanceDataCount;
			const SubMeshInstanceData *m_instanceData;
			const SubMeshInfo *m_subMeshInfo;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshInfoBufferInfo;
			//BufferViewHandle m_indicesBufferHandle;
			//BufferViewHandle m_indirectBufferHandle;
			ImageViewHandle m_voxelSceneImageHandle;
			ImageViewHandle m_voxelSceneOpacityImageHandle;
			ImageViewHandle m_shadowImageViewHandle;
			VkDescriptorBufferInfo m_lightDataBufferInfo;
			VkDescriptorBufferInfo m_shadowMatricesBufferInfo;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}