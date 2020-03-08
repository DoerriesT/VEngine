#pragma once
#include "Graphics/RenderGraph2.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext2;

	class GTAOModule2
	{
	public:

		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_tangentSpaceImageViewHandle;
			rg::ImageViewHandle m_velocityImageViewHandle;
		};

		explicit GTAOModule2(gal::GraphicsDevice *graphicsDevice, uint32_t width, uint32_t height);
		GTAOModule2(const GTAOModule2 &) = delete;
		GTAOModule2(const GTAOModule2 &&) = delete;
		GTAOModule2 &operator= (const GTAOModule2 &) = delete;
		GTAOModule2 &operator= (const GTAOModule2 &&) = delete;
		~GTAOModule2();

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
		void resize(uint32_t width, uint32_t height);
		rg::ImageViewHandle getAOResultImageViewHandle();

	private:
		gal::GraphicsDevice *m_graphicsDevice;
		uint32_t m_width = 1;
		uint32_t m_height = 1;
		rg::ImageViewHandle m_gtaoImageViewHandle = 0;

		gal::Image *m_gtaoHistoryTextures[RendererConsts::FRAMES_IN_FLIGHT] = {};

		rg::ResourceStateData m_gtaoHistoryTextureState[RendererConsts::FRAMES_IN_FLIGHT] = {};
	};
}