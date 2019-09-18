#pragma once
#include "Graphics/Vulkan/RenderGraph.h"

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
			ImageViewHandle m_resultImageViewHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}