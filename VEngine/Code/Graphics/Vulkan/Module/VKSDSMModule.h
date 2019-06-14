#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;
	struct RenderData;

	namespace VKSDSMModule
	{
		struct InputData
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			float m_nearPlane;
			float m_farPlane;
			glm::mat4 m_invProjection;
			FrameGraph::ImageHandle m_depthImageHandle;
		};

		struct OutputData
		{
			FrameGraph::BufferHandle m_partitionBoundsBufferHandle;
			FrameGraph::BufferHandle m_splitsBufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const InputData &inData, OutputData &outData);
	};
}