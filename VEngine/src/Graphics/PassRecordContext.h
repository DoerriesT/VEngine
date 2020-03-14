#pragma once

namespace VEngine
{
	struct RenderResources;
	class PipelineCache;
	class DescriptorSetCache;
	struct CommonRenderData;

	struct PassRecordContext
	{
		RenderResources *m_renderResources;
		PipelineCache *m_pipelineCache;
		DescriptorSetCache *m_descriptorSetCache;
		const CommonRenderData *m_commonRenderData;
	};
}