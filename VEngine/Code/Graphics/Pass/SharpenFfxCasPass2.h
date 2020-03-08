#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace SharpenFfxCasPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			bool m_gammaSpaceInput;
			float m_sharpness;
			rg::ImageViewHandle m_srcImageHandle;
			rg::ImageViewHandle m_dstImageHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}