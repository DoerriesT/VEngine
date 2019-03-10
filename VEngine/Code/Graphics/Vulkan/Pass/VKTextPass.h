#pragma once
#include "Graphics/Vulkan/FrameGraph/FrameGraph.h"

namespace VEngine
{
	struct VKRenderResources;

	class VKTextPass : FrameGraph::Pass
	{
	public:
		struct String
		{
			const char *m_chars;
			size_t m_positionX;
			size_t m_positionY;
		};

		explicit VKTextPass(VkPipeline pipeline,
			VkPipelineLayout pipelineLayout,
			VKRenderResources *renderResources,
			uint32_t width,
			uint32_t height,
			uint32_t atlasTextureIndex,
			size_t stringCount,
			const String *strings);

		void addToGraph(FrameGraph::Graph &graph, FrameGraph::ImageHandle colorTextureHandle);
		void record(VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry) override;

	private:
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
		VKRenderResources *m_renderResources;
		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_atlasTextureIndex;
		size_t m_stringCount;
		const String *m_strings;
	};
}