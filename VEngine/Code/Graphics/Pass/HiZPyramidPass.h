#pragma once
#include "Graphics/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace HiZPyramidPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
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

		void addToGraph(rg::RenderGraph &graph, const Data &data, OutData &outData);
	}
}