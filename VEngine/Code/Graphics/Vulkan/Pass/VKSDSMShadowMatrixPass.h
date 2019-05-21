#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;

	namespace VKSDSMShadowMatrixPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			glm::mat4 m_cameraViewToLightView;
			glm::mat4 m_lightView;
			float m_nearPlane;
			float m_farPlane;
			float m_projScaleXInv;
			float m_projScaleYInv;
			float m_lightSpaceNear;
			float m_lightSpaceFar;
			FrameGraph::BufferHandle m_shadowDataBufferHandle;
			FrameGraph::BufferHandle m_depthBoundsBufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}