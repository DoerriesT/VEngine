#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	class VKPipelineCache;
	struct VKRenderResources;

	namespace VKTextPass
	{
		struct String
		{
			const char *m_chars;
			size_t m_positionX;
			size_t m_positionY;
		};

		struct Data
		{
			VKRenderResources *m_renderResources;
			VKPipelineCache *m_pipelineCache;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_atlasTextureIndex;
			size_t m_stringCount;
			const String *m_strings;
			FrameGraph::ImageHandle m_colorImageHandle;
		};

		void addToGraph(FrameGraph::Graph &graph, const Data &data);
	}
}