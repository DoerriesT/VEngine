#pragma once
#include "Graphics/Vulkan/RenderGraph.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext;

	class SSRModule
	{
	public:
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_noiseTextureHandle;
			ImageViewHandle m_hiZPyramidImageViewHandle;
			ImageViewHandle m_normalImageViewHandle;
			ImageViewHandle m_depthImageViewHandle;
			ImageViewHandle m_albedoImageViewHandle;
			ImageViewHandle m_prevColorImageViewHandle;
			ImageViewHandle m_velocityImageViewHandle;
		};

		explicit SSRModule(uint32_t width, uint32_t height);
		SSRModule(const SSRModule &) = delete;
		SSRModule(const SSRModule &&) = delete;
		SSRModule &operator= (const SSRModule &) = delete;
		SSRModule &operator= (const SSRModule &&) = delete;
		~SSRModule();

		void addToGraph(RenderGraph &graph, const Data &data);
		void resize(uint32_t width, uint32_t height);
		ImageViewHandle getSSRResultImageViewHandle();

	private:
		uint32_t m_width = 1;
		uint32_t m_height = 1;
		ImageViewHandle m_ssrImageViewHandle = 0;

		VkImage m_ssrHistoryImages[RendererConsts::FRAMES_IN_FLIGHT] = {};

		VKAllocationHandle m_ssrHistoryImageAllocHandles[RendererConsts::FRAMES_IN_FLIGHT] = {};

		VkQueue m_ssrHistoryImageQueue[RendererConsts::FRAMES_IN_FLIGHT] = {};
		ResourceState m_ssrHistoryImageResourceState[RendererConsts::FRAMES_IN_FLIGHT] = {};
	};
}