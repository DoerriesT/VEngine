#pragma once
#include <vulkan/vulkan.h>

namespace VEngine
{
	struct VKRenderResources;

	class VKMemoryHeapDebugPipeline
	{
	public:
		explicit VKMemoryHeapDebugPipeline(VkDevice device, VkFormat attachmentFormat);
		VKMemoryHeapDebugPipeline(const VKMemoryHeapDebugPipeline &) = delete;
		VKMemoryHeapDebugPipeline(const VKMemoryHeapDebugPipeline &&) = delete;
		VKMemoryHeapDebugPipeline &operator= (const VKMemoryHeapDebugPipeline &) = delete;
		VKMemoryHeapDebugPipeline &operator= (const VKMemoryHeapDebugPipeline &&) = delete;
		~VKMemoryHeapDebugPipeline();
		VkPipeline getPipeline() const;
		VkPipelineLayout getLayout() const;

	private:
		VkDevice m_device;
		VkPipeline m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}
