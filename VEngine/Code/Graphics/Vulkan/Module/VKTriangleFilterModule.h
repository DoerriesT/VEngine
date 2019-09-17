//#pragma once
//#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
//#include <glm/mat4x4.hpp>
//
//namespace VEngine
//{
//	class VKPipelineCache;
//	class VKDescriptorSetCache;
//	struct VKRenderResources;
//	struct RenderData;
//	struct SubMeshInfo;
//	struct SubMeshInstanceData;
//
//	namespace VKTriangleFilterModule
//	{
//		struct InputData
//		{
//			VKRenderResources *m_renderResources;
//			VKPipelineCache *m_pipelineCache;
//			VKDescriptorSetCache *m_descriptorSetCache;
//			const SubMeshInstanceData *m_instanceData;
//			const SubMeshInfo *m_subMeshInfoList;
//			VkDescriptorBufferInfo m_instanceDataBufferInfo;
//			uint32_t m_width;
//			uint32_t m_height;
//			uint32_t m_currentResourceIndex;
//			uint32_t m_viewProjectionBufferOffset;
//			uint32_t m_instanceCount;
//			uint32_t m_instanceOffset;
//			bool m_cullBackface;
//			VkDescriptorBufferInfo m_transformDataBufferInfo;
//			FrameGraph::BufferHandle m_viewProjectionBufferHandle;
//		};
//
//		struct OutputData
//		{
//			FrameGraph::BufferHandle m_indirectBufferHandle;
//			FrameGraph::BufferHandle m_drawCountsBufferHandle;
//			FrameGraph::BufferHandle m_filteredIndicesBufferHandle;
//		};
//
//		void addToGraph(FrameGraph::Graph &graph, const InputData &inData, OutputData &outData);
//	}
//}