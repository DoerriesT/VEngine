#pragma once

namespace VEngine
{
	struct VKRenderResources;
	class DeferredObjectDeleter;
	class RenderPassCache;
	class VKPipelineCache;
	class VKDescriptorSetCache;
	struct CommonRenderData;

	struct PassRecordContext
	{
		VKRenderResources *m_renderResources;
		DeferredObjectDeleter *m_deferredObjectDeleter;
		RenderPassCache *m_renderPassCache;
		VKPipelineCache *m_pipelineCache;
		VKDescriptorSetCache *m_descriptorSetCache;
		const CommonRenderData *m_commonRenderData;
	};
}