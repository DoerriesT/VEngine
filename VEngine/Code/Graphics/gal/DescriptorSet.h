#pragma once
#include <cstdint>
#include "Common.h"

namespace VEngine
{
	namespace gal
	{
		class DescriptorSetLayout
		{
		public:
			virtual ~DescriptorSetLayout() = default;
			virtual void *getNativeHandle() const = 0;
			virtual const DescriptorSetLayoutTypeCounts &getTypeCounts() const = 0;
		};

		class DescriptorSet
		{
		public:
			virtual ~DescriptorSet() = default;
			virtual void *getNativeHandle() const = 0;
			virtual void update(uint32_t count, const DescriptorSetUpdate *updates) = 0;
		};

		class DescriptorPool
		{
		public:
			virtual ~DescriptorPool() = default;
			virtual void *getNativeHandle() const = 0;
			virtual void allocateDescriptorSets(uint32_t count, const DescriptorSetLayout **layouts, DescriptorSet **sets) = 0;
			virtual void freeDescriptorSets(uint32_t count, DescriptorSet **sets) = 0;
		};
	}
}