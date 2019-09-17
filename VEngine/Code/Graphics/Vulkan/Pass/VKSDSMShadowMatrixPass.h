#pragma once
#include <glm/mat4x4.hpp>
#include "Graphics/Vulkan/RenderGraph.h"

namespace VEngine
{
	struct PassRecordContext;

	namespace VKSDSMShadowMatrixPass
	{
		struct Data
		{
			PassRecordContext *m_passRecordContext;
			glm::mat4 m_lightView;
			float m_lightSpaceNear;
			float m_lightSpaceFar;
			BufferViewHandle m_shadowDataBufferHandle;
			BufferViewHandle m_partitionBoundsBufferHandle;
		};

		void addToGraph(RenderGraph &graph, const Data &data);
	}
}