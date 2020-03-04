#pragma once

namespace VEngine
{
	namespace gal
	{
		class QueryPool
		{
		public:
			virtual ~QueryPool() = 0;
			virtual void *getNativeHandle() const = 0;
		};
	}
}