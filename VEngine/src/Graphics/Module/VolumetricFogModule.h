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
			bool m_ignoreHistory;
			const CommonRenderData *m_commonData;
			rg::ImageViewHandle m_shadowImageViewHandle;
			rg::ImageViewHandle m_shadowAtlasImageViewHandle;
			rg::BufferViewHandle m_exposureDataBufferHandle;
			rg::BufferViewHandle m_punctualLightsBitMaskBufferHandle;
			rg::BufferViewHandle m_punctualLightsShadowedBitMaskBufferHandle;
			rg::BufferViewHandle m_localMediaBitMaskBufferHandle;
			gal::DescriptorBufferInfo m_directionalLightsBufferInfo;
			gal::DescriptorBufferInfo m_directionalLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsZBinsBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedBufferInfo;
			gal::DescriptorBufferInfo m_punctualLightsShadowedZBinsBufferInfo;
			gal::DescriptorBufferInfo m_localMediaBufferInfo;
			gal::DescriptorBufferInfo m_localMediaZBinsBufferInfo;
			gal::DescriptorBufferInfo m_globalMediaBufferInfo;
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
		static constexpr size_t s_haltonSampleCount = 32;
		gal::GraphicsDevice *m_graphicsDevice;
		uint32_t m_width = 1;
		uint32_t m_height = 1;
		float *m_haltonJitter;
		rg::ImageViewHandle m_volumetricScatteringImageViewHandle = 0;

		gal::Image *m_inScatteringHistoryImages[RendererConsts::FRAMES_IN_FLIGHT] = {};

		rg::ResourceStateData m_inScatteringHistoryImageState[RendererConsts::FRAMES_IN_FLIGHT] = {};
	};
}