#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKGeometryPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_drawCount;
			bool m_alphaMasked;
			glm::mat4 m_viewMatrix;
			glm::mat4 m_jitteredViewProjectionMatrix;
			VkDescriptorBufferInfo m_instanceDataBufferInfo;
			VkDescriptorBufferInfo m_materialDataBufferInfo;
			VkDescriptorBufferInfo m_transformDataBufferInfo;
			FrameGraph::BufferHandle m_indirectBufferHandle;
			FrameGraph::ImageHandle m_depthImageHandle;
			FrameGraph::ImageHandle m_uvImageHandle;
			FrameGraph::ImageHandle m_ddxyLengthImageHandle;
			FrameGraph::ImageHandle m_ddxyRotMaterialIdImageHandle;
			FrameGraph::ImageHandle m_tangentSpaceImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}