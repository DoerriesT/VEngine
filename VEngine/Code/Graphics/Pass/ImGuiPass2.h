#pragma once
#include "Graphics/RenderGraph2.h"

struct ImDrawData;

namespace VEngine
{
	struct PassRecordContext2;

	namespace ImGuiPass2
	{
		struct Data
		{
			PassRecordContext2 *m_passRecordContext;
			ImDrawData *m_imGuiDrawData;
			rg::ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(rg::RenderGraph2 &graph, const Data &data);
	}
}