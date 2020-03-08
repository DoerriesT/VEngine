#pragma once
#include "Graphics/RenderGraph2.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext2;

	class SSRModule2
	{
	public:
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			uint32_t m_noiseTextureHandle;
			rg::ImageViewHandle m_hiZPyramidImageViewHandle;
			rg::ImageViewHandle m_normalImageViewHandle;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_albedoImageViewHandle;
			rg::ImageViewHandle m_prevColorImageViewHandle;
			rg::ImageViewHandle m_velocityImageViewHandle;
		};

		explicit SSRModule2(gal::GraphicsDevice *graphicsDevice, uint32_t width, uint32_t height);
		SSRModule2(const SSRModule2 &) = delete;
		SSRModule2(const SSRModule2 &&) = delete;
		SSRModule2 &operator= (const SSRModule2 &) = delete;
		SSRModule2 &operator= (const SSRModule2 &&) = delete;
		~SSRModule2();

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
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