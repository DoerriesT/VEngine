#pragma once
#include "Graphics/RenderGraph.h"
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
			rg::ImageViewHandle m_hiZPyramidImageViewHandle;
			rg::ImageViewHandle m_normalImageViewHandle;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_albedoImageViewHandle;
			rg::ImageViewHandle m_prevColorImageViewHandle;
			rg::ImageViewHandle m_velocityImageViewHandle;
		};

		explicit SSRModule(gal::GraphicsDevice *graphicsDevice, uint32_t width, uint32_t height);
		SSRModule(const SSRModule &) = delete;
		SSRModule(const SSRModule &&) = delete;
		SSRModule &operator= (const SSRModule &) = delete;
		SSRModule &operator= (const SSRModule &&) = delete;
		~SSRModule();

		void addToGraph(rg::RenderGraph &graph, const Data &data);
		void resize(uint32_t width, uint32_t height);
		rg::ImageViewHandle getSSRResultImageViewHandle();

	private:
		gal::GraphicsDevice *m_graphicsDevice;
		uint32_t m_width = 1;
		uint32_t m_height = 1;
		rg::ImageViewHandle m_ssrImageViewHandle = 0;

		gal::Image *m_ssrHistoryImages[RendererConsts::FRAMES_IN_FLIGHT] = {};
		rg::ResourceStateData m_ssrHistoryImageState[RendererConsts::FRAMES_IN_FLIGHT] = {};
	};
}