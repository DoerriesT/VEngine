#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct DirectionalLightData;
	struct CommonRenderData;

	class VolumetricFogModule
	{
	public:

		struct Data
		{
			PassRecordContext *m_passRecordContext;
			const DirectionalLightData *m_lightData;
			const CommonRenderData *m_commonData;
			rg::ImageViewHandle m_shadowImageViewHandle;
			gal::DescriptorBufferInfo m_shadowMatricesBufferInfo;
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
		gal::GraphicsDevice *m_graphicsDevice;
		uint32_t m_width = 1;
		uint32_t m_height = 1;
		rg::ImageViewHandle m_volumetricScatteringImageViewHandle = 0;
	};
}