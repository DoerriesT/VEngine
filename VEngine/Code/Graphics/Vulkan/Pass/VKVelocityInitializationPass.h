#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKVelocityInitializationPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			glm::mat4 m_reprojectionMatrix;
			FrameGraph::ImageHandle m_depthImageHandle;
			FrameGraph::ImageHandle m_velocityImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}