#pragma once
#include <cstdint>

namespace VEngine
{
	namespace gal
	{
		class DescriptorSetLayout;

		class GraphicsPipeline
		{
		public:
			virtual ~GraphicsPipeline() = 0;
			virtual void *getNativeHandle() const = 0;
			virtual uint32_t getDescriptorSetLayoutCount() const = 0;
			virtual const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const = 0;
		};

		class ComputePipeline
		{
		public:
			virtual ~ComputePipeline() = 0;
			virtual void *getNativeHandle() const = 0;
			virtual uint32_t getDescriptorSetLayoutCount() const = 0;
			virtual const DescriptorSetLayout *getDescriptorSetLayout(uint32_t index) const = 0;
		};
	}
}