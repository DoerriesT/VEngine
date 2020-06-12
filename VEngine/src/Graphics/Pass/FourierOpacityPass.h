#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;
	struct FOMAtlasDrawInfo;

	namespace FourierOpacityPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			uint32_t m_drawCount;
			uint32_t m_particleCount;
			const FOMAtlasDrawInfo *m_drawInfo;
			gal::DescriptorBufferInfo m_localMediaBufferInfo;
			gal::DescriptorBufferInfo m_globalMediaBufferInfo;
			gal::DescriptorBufferInfo m_particleBufferInfo;
			rg::ImageViewHandle m_fomImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}