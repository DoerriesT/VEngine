#pragma once
#include "Common.h"

namespace VEngine
{
	namespace gal
	{
		class Sampler
		{
		public:
			virtual ~Sampler() = 0;
			virtual void *getNativeHandle() const = 0;
		};

		class Image
		{
		public:
			virtual ~Image() = 0;
			virtual void *getNativeHandle() const = 0;
			virtual const ImageCreateInfo &getDescription() const = 0;
		};

		class Buffer
		{
		public:
			virtual ~Buffer() = 0;
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
			virtual ~ImageView() = 0;
			virtual void *getNativeHandle() const = 0;
			virtual const Image *getImage() const = 0;
		};

		class BufferView
		{
		public:
			virtual ~BufferView() = 0;
			virtual void *getNativeHandle() const = 0;
			virtual const Buffer *getBuffer() const = 0;
		};
	}
}