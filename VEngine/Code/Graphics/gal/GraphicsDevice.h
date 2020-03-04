#pragma once
#include "Common.h"

namespace VEngine
{
	namespace gal
	{
		struct GraphicsPipelineCreateInfo;
		struct ComputePipelineCreateInfo;
		class GraphicsPipeline;
		class ComputePipeline;
		class Queue;
		class CommandListPool;
		class QueryPool;
		class Image;
		class Buffer;
		class Semaphore;
		class DescriptorPool;
		class ImageView;
		class Sampler;
		class SwapChain;

		class GraphicsDevice
		{
		public:
			GraphicsDevice *create(void *windowHandle, bool debugLayer, GraphicsBackend backend);
			virtual ~GraphicsDevice() = 0;
			virtual void createGraphicsPipelines(uint32_t count, const GraphicsPipelineCreateInfo *createInfo, GraphicsPipeline **pipelines) = 0;
			virtual void createComputePipelines(uint32_t count, const ComputePipelineCreateInfo *createInfo, ComputePipeline **pipelines) = 0;
			virtual void destroyGraphicsPipeline(GraphicsPipeline *pipeline) = 0;
			virtual void destroyComputePipeline(ComputePipeline *pipeline) = 0;
			virtual void createCommandListPool(const Queue *queue, CommandListPool **commandListPool) = 0;
			virtual void destroyCommandListPool(CommandListPool *commandListPool) = 0;
			virtual void createQueryPool(QueryType queryType, uint32_t queryCount, QueryPool **queryPool) = 0;
			virtual void destroyQueryPool(QueryPool *queryPool) = 0;
			virtual void createImage(const ImageCreateInfo &imageCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Image **image) = 0;
			virtual void createBuffer(const BufferCreateInfo &bufferCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Buffer **buffer) = 0;
			virtual void destroyImage(Image *image) = 0;
			virtual void destroyBuffer(Buffer *buffer) = 0;
			virtual void createImageView(const ImageViewCreateInfo &imageViewCreateInfo, ImageView **imageView) = 0;
			virtual void createBufferView(const BufferViewCreateInfo &bufferViewCreateInfo, BufferView **bufferView) = 0;
			virtual void destroyImageView(ImageView *imageView) = 0;
			virtual void destroyBufferView(BufferView *bufferView) = 0;
			virtual void createSampler(const SamplerCreateInfo &samplerCreateInfo, Sampler **sampler) = 0;
			virtual void destroySampler(Sampler *sampler) = 0;
			virtual void createSemaphore(uint64_t initialValue, Semaphore **semaphore) = 0;
			virtual void destroySemaphore(Semaphore *semaphore) = 0;
			virtual void createDescriptorPool(uint32_t maxSets, const uint32_t typeCounts[(size_t)DescriptorType::RANGE_SIZE], DescriptorPool **descriptorPool) = 0;
			virtual void destroyDescriptorPool(DescriptorPool *descriptorPool) = 0;
			virtual void createSwapChain(const Queue *presentQueue, uint32_t width, uint32_t height, SwapChain **swapChain) = 0;
			virtual void destroySwapChain() = 0;
		};
	}
}
