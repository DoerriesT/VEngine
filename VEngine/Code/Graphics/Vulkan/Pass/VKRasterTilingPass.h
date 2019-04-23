#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"
#include <glm/mat4x4.hpp>

namespace VEngine
{
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct VKRenderResources;
	struct LightData;

	namespace VKRasterTilingPass
	{
		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			VKDescriptorSetCache *m_descriptorSetCache;
			uint32_t m_width;
			uint32_t m_height;
			const LightData *m_lightData;
			glm::mat4 m_projection;
			FrameGraph::BufferHandle m_pointLightBitMaskBufferHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}