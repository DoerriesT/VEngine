#pragma once
#include <cstdint>

namespace VEngine
{
	namespace gal
	{
		struct SubmitInfo;

		class Queue
		{
		public:
			virtual ~Queue() = default;
			virtual void *getNativeHandle() const = 0;
			virtual uint32_t getFamilyIndex() const = 0;
			virtual void submit(uint32_t count, const SubmitInfo *submitInfo) = 0;
		};
	}
}