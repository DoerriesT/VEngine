#pragma once
#include <cstdint>

namespace VEngine
{
	struct ViewRenderList
	{
		uint32_t m_opaqueOffset = 0;
		uint32_t m_opaqueCount = 0;
		uint32_t m_maskedOffset = 0;
		uint32_t m_maskedCount = 0;
		uint32_t m_transparentOffset = 0;
		uint32_t m_transparentCount = 0;
	};
}