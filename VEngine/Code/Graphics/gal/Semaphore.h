#pragma once
#include <cstdint>

namespace VEngine
{
	namespace gal
	{
		class Semaphore
		{
		public:
			virtual ~Semaphore() = default;
			virtual void *getNativeHandle() const = 0;
			virtual uint64_t getCompletedValue() const = 0;
			virtual void wait(uint64_t waitValue) const = 0;
			virtual void signal(uint64_t signalValue) const = 0;
		};
	}
}