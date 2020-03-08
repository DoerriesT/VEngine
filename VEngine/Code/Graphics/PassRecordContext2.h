#pragma once

namespace VEngine
{
	struct RenderResources;
	class PipelineCache;
	class DescriptorSetCache2;
	struct CommonRenderData;

	struct PassRecordContext2
	{
		RenderResources *m_renderResources;
		PipelineCache *m_pipelineCache;
		DescriptorSetCache2 *m_descriptorSetCache;
		const CommonRenderData *m_commonRenderData;
	};
}