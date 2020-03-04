#pragma once
#include <cstdint>
#include "Common.h"

namespace VEngine
{
	namespace gal
	{
		class Semaphore;

		class SwapChain
		{
		public:
			virtual ~SwapChain() = 0;
			virtual void *getNativeHandle() const = 0;
			virtual void resize(uint32_t width, uint32_t height) = 0;
			virtual void getCurrentImageIndex(uint32_t &currentImageIndex, Semaphore *signalSemaphore, uint64_t semaphoreSignalValue) = 0;
			virtual void present(Semaphore *waitSemaphore, uint64_t semaphoreWaitValue) = 0;
			virtual Extent2D getExtent() const = 0;
			virtual Extent2D getRecreationExtent() const = 0;
			virtual Format getImageFormat() const = 0;
			virtual Image *getImage(size_t index) const = 0;
			virtual Queue *getPresentQueue() const = 0;
		};
	}
}