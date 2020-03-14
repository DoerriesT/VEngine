#pragma once
#include "Graphics/RenderGraph.h"
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
			rg::ImageViewHandle m_depthImageViewHandle;
			rg::ImageViewHandle m_tangentSpaceImageViewHandle;
			rg::ImageViewHandle m_velocityImageViewHandle;
		};

		explicit GTAOModule(gal::GraphicsDevice *graphicsDevice, uint32_t width, uint32_t height);
		GTAOModule(const GTAOModule &) = delete;
		GTAOModule(const GTAOModule &&) = delete;
		GTAOModule &operator= (const GTAOModule &) = delete;
		GTAOModule &operator= (const GTAOModule &&) = delete;
		~GTAOModule();

		void addToGraph(rg::RenderGraph &graph, const Data &data);
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