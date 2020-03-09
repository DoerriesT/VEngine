#pragma once
#include "Graphics/RenderGraph.h"

struct ImDrawData;

namespace VEngine
{
	struct PassRecordContext;

	namespace ImGuiPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			ImDrawData *m_imGuiDrawData;
			rg::ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(rg::RenderGraph &graph, const Data &data);
	}
}