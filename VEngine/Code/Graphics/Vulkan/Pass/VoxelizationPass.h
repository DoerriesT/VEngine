#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VoxelizationPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			VkDescriptorBufferInfo m_subMeshInfoBufferInfo;
			BufferViewHandle m_indicesBufferHandle;
			BufferViewHandle m_indirectBufferHandle;
			ImageViewHandle m_voxelSceneImageHandle;
			ImageViewHandle m_voxelSceneOpacityImageHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}