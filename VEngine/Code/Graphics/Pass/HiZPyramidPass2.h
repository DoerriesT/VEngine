#pragma once
#include "Graphics/RenderGraph2.h"

namespace VEngine
{
	struct PassRecordContext2;

	namespace HiZPyramidPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			rg::ImageViewHandle m_inputImageViewHandle;
			bool m_maxReduction; // set to true to use max(); uses min() otherwise
			bool m_copyFirstLevel;
			bool m_forceExecution;
		};

		struct OutData
		{
			rg::ImageHandle m_resultImageHandle;
			rg::ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data, OutData &outData);
	}
}