#pragma once

namespace VEngine
{
	struct PassTimingInfo
	{
		const char *m_passName;
		float m_passTime;
		float m_passTimeWithSync;
	};
}