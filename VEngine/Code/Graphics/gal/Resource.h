#pragma once
#include "Common.h"

namespace VEngine
{
	namespace gal
	{
		class Sampler
		{
		public:
			virtual ~Sampler() = default;
			virtual void *getNativeHandle() const = 0;
		};

		class Image
		{
		public:
			virtual ~Image() = default;
			virtual void *getNativeHandle() const = 0;
			virtual const ImageCreateInfo &getDescription() const = 0;
		};

		class Buffer
		{
		public:
			virtual ~Buffer() = default;
			virtual void *getNativeHandle() const = 0;
			virtual const BufferCreateInfo &getDescription() const = 0;
			virtual void map(void **data) = 0;
			virtual void unmap() = 0;
			virtual void invalidate(uint32_t count, const MemoryRange *ranges) = 0;
			virtual void flush(uint32_t count, const MemoryRange *ranges) = 0;
		};

		class ImageView
		{
		public:
			virtual ~ImageView() = default;
			virtual void *getNativeHandle() const = 0;
			virtual const Image *getImage() const = 0;
		};

		class BufferView
		{
		public:
			virtual ~BufferView() = default;
			virtual void *getNativeHandle() const = 0;
			virtual const Buffer *getBuffer() const = 0;
		};
	}
}