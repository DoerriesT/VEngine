#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKGTAOTemporalFilterPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			float m_nearPlane;
			float m_farPlane;
			glm::mat4 m_invViewProjection;
			glm::mat4 m_prevInvViewProjection;
			FrameGraph::ImageHandle m_inputImageHandle;
			FrameGraph::ImageHandle m_velocityImageHandle;
			FrameGraph::ImageHandle m_previousImageHandle;
			FrameGraph::ImageHandle m_resultImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}