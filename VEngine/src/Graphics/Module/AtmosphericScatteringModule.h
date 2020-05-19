#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	class AtmosphericScatteringModule
	{
	public:

		struct Data
		{
			PassRecordContext *m_passRecordContext;
		};

		explicit AtmosphericScatteringModule(gal::GraphicsDevice *graphicsDevice);
		AtmosphericScatteringModule(const AtmosphericScatteringModule &) = delete;
		AtmosphericScatteringModule(const AtmosphericScatteringModule &&) = delete;
		AtmosphericScatteringModule &operator= (const AtmosphericScatteringModule &) = delete;
		AtmosphericScatteringModule &operator= (const AtmosphericScatteringModule &&) = delete;
		~AtmosphericScatteringModule();

		void addPrecomputationToGraph(rg::RenderGraph &graph, const Data &data);
		void registerResources(rg::RenderGraph &graph);
		rg::ImageViewHandle getTransmittanceImageViewHandle();
		rg::ImageViewHandle getScatteringImageViewHandle();
		gal::DescriptorBufferInfo getConstantBufferInfo();

	private:
		gal::GraphicsDevice *m_graphicsDevice;
		
		gal::Image *m_transmittanceImage = nullptr;
		gal::Image *m_scatteringImage = nullptr;
		gal::Image *m_irradianceImage = nullptr;
		gal::Buffer *m_constantBuffer = nullptr;

		rg::ImageViewHandle m_transmittanceImageViewHandle = 0;
		rg::ImageViewHandle m_scatteringImageViewHandle = 0;

		rg::ResourceStateData m_transmittanceImageState = {};
		rg::ResourceStateData m_scatteringImageState = {};
		rg::ResourceStateData m_irradianceImageState = {};
	};
}