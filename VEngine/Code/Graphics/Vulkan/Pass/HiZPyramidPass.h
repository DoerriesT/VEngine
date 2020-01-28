#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace HiZPyramidPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImageViewHandle m_inputImageViewHandle;
			bool m_maxReduction; // set to true to use max(); uses min() otherwise
			bool m_copyFirstLevel;
			bool m_forceExecution;
		};

		struct OutData
		{
			ImageHandle m_resultImageHandle;
			ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data, OutData &outData);
	}
}