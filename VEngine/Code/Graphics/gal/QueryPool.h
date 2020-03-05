#pragma once

namespace VEngine
{
	namespace gal
	{
		class QueryPool
		{
		public:
			virtual ~QueryPool() = default;
			virtual void *getNativeHandle() const = 0;
		};
	}
}