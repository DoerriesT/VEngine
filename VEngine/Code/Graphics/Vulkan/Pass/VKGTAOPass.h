#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKGTAOPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_frame;
			float m_fovy;
			glm::mat4 m_invProjection;
			FrameGraph::ImageHandle m_depthImageHandle;
			FrameGraph::ImageHandle m_resultImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}