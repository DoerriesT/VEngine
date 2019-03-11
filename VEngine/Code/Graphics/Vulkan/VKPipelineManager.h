#pragma once
#include "VKPipeline.h"
#include <unordered_map>

namespace VEngine
{
	class VKPipelineManager
	{
	public:
		struct PipelineLayoutPair
		{
			VkPipeline m_pipeline;
			VkPipelineLayout m_layout;
		};

		PipelineLayoutPair getPipeline(const VKGraphicsPipelineDescription &pipelineDesc, VkRenderPass renderPass);
		PipelineLayoutPair getPipeline(const VKComputePipelineDescription &pipelineDesc);

	private:
		std::unordered_map<VKGraphicsPipelineDescription, PipelineLayoutPair, VKGraphicsPipelineDescriptionHash> m_graphicsPipelines;
		std::unordered_map<VKComputePipelineDescription, PipelineLayoutPair, VKComputePipelineDescriptionHash> m_computePipelines;
	};
}
