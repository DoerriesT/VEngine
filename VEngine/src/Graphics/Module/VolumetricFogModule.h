#pragma once
#include "Graphics/RenderGraph.h"
#include "Graphics/RendererConsts.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLight;
	struct CommonRenderData;

	class VolumetricFogModule
	{
	public:

		struct Data
		{
			PassRecordContext *m_passRecordContext;
			const DirectionalLight *m_lightData;
			const CommonRenderData *m_commonData;
			rg::ImageViewHandle m_shadowImageViewHandle;
			rg::BufferViewHandle m_pointLightBitMaskBufferHandle;
			rg::BufferViewHandle m_spotLightBitMaskBufferHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
			gal::DescriptorBufferInfo m_pointLightDataBufferInfo;
			gal::DescriptorBufferInfo m_pointLightZBinsBufferInfo;
			gal::DescriptorBufferInfo m_spotLightDataBufferInfo;
			gal::DescriptorBufferInfo m_spotLightZBinsBufferInfo;
		};

		explicit VolumetricFogModule(gal::GraphicsDevice *graphicsDevice, uint32_t width, uint32_t height);
		VolumetricFogModule(const VolumetricFogModule &) = delete;
		VolumetricFogModule(const VolumetricFogModule &&) = delete;
		VolumetricFogModule &operator= (const VolumetricFogModule &) = delete;
		VolumetricFogModule &operator= (const VolumetricFogModule &&) = delete;
		~VolumetricFogModule();

		void addToGraph(rg::RenderGraph &graph, const Data &data);
		void resize(uint32_t width, uint32_t height);
		rg::ImageViewHandle getVolumetricScatteringImageViewHandle();

	private:
		static constexpr size_t s_haltonSampleCount = 64;
		gal::GraphicsDevice *m_graphicsDevice;
		uint32_t m_width = 1;
		uint32_t m_height = 1;
		float *m_haltonJitter;
		rg::ImageViewHandle m_volumetricScatteringImageViewHandle = 0;

		gal::Image *m_inScatteringHistoryImages[RendererConsts::FRAMES_IN_FLIGHT] = {};

		rg::ResourceStateData m_inScatteringHistoryImageState[RendererConsts::FRAMES_IN_FLIGHT] = {};
	};
}