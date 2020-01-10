#pragma once
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext;

	class GTAOModule
	{
	public:

		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_depthImageViewHandle;
			ImageViewHandle m_tangentSpaceImageViewHandle;
			ImageViewHandle m_velocityImageViewHandle;
		};

		explicit GTAOModule(uint32_t width, uint32_t height);
		GTAOModule(const GTAOModule &) = delete;
		GTAOModule(const GTAOModule &&) = delete;
		GTAOModule &operator= (const GTAOModule &) = delete;
		GTAOModule &operator= (const GTAOModule &&) = delete;
		~GTAOModule();

		void addToGraph(RenderGraph &graph, const Data &data);
		void resize(uint32_t width, uint32_t height);
		ImageViewHandle getAOResultImageViewHandle();

	private:
		uint32_t m_width = 1;
		uint32_t m_height = 1;
		ImageViewHandle m_gtaoImageViewHandle = 0;

		VkImage m_gtaoHistoryTextures[RendererConsts::FRAMES_IN_FLIGHT] = {};

		VKAllocationHandle m_gtaoHistoryTextureAllocHandles[RendererConsts::FRAMES_IN_FLIGHT] = {};

		VkQueue m_gtaoHistoryTextureQueue[RendererConsts::FRAMES_IN_FLIGHT] = {};
		ResourceState m_gtaoHistoryTextureResourceState[RendererConsts::FRAMES_IN_FLIGHT] = {};
	};
}