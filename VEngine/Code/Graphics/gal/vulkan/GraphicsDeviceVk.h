#pragma once
#include "Graphics/gal/GraphicsDevice.h"
#include "QueueVk.h"
#include "Utility/ObjectPool.h"
#include "PipelineVk.h"
#include "CommandListPoolVk.h"
#include "ResourceVk.h"
#include "SemaphoreVk.h"
#include "QueryPoolVk.h"
#include "DescriptorSetVk.h"
#include "SwapChainVk2.h"

namespace VEngine
{
	namespace gal
	{
		class RenderPassCacheVk;
		class MemoryAllocatorVk;
		struct RenderPassDescriptionVk;

		class GraphicsDeviceVk : public GraphicsDevice
		{
		public:
			explicit GraphicsDeviceVk(void *windowHandle, bool debugLayer);
			~GraphicsDeviceVk();
			void createGraphicsPipelines(uint32_t count, const GraphicsPipelineCreateInfo *createInfo, GraphicsPipeline **pipelines) override;
			void createComputePipelines(uint32_t count, const ComputePipelineCreateInfo *createInfo, ComputePipeline **pipelines) override;
			void destroyGraphicsPipeline(GraphicsPipeline *pipeline) override;
			void destroyComputePipeline(ComputePipeline *pipeline) override;
			void createCommandListPool(const Queue *queue, CommandListPool **commandListPool) override;
			void destroyCommandListPool(CommandListPool *commandListPool) override;
			void createQueryPool(QueryType queryType, uint32_t queryCount, QueryPool **queryPool) override;
			void destroyQueryPool(QueryPool *queryPool) override;
			void createImage(const ImageCreateInfo &imageCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Image **image) override;
			void createBuffer(const BufferCreateInfo &bufferCreateInfo, MemoryPropertyFlags requiredMemoryPropertyFlags, MemoryPropertyFlags preferredMemoryPropertyFlags, bool dedicated, Buffer **buffer) override;
			void destroyImage(Image *image) override;
			void destroyBuffer(Buffer *buffer) override;
			void createImageView(const ImageViewCreateInfo &imageViewCreateInfo, ImageView **imageView);
			void createBufferView(const BufferViewCreateInfo &bufferViewCreateInfo, BufferView **bufferView);
			void destroyImageView(ImageView *imageView);
			void destroyBufferView(BufferView *bufferView);
			void createSampler(const SamplerCreateInfo &samplerCreateInfo, Sampler **sampler);
			void destroySampler(Sampler *sampler);
			void createSemaphore(uint64_t initialValue, Semaphore **semaphore) override;
			void destroySemaphore(Semaphore *semaphore) override;
			void createDescriptorPool(uint32_t maxSets, const uint32_t typeCounts[(size_t)DescriptorType::RANGE_SIZE], DescriptorPool **descriptorPool) override;
			void destroyDescriptorPool(DescriptorPool *descriptorPool) override;
			void createSwapChain(const Queue *presentQueue, uint32_t width, uint32_t height, SwapChain **swapChain);
			void destroySwapChain();
			VkDevice getDevice() const;
			void addToDeletionQueue(VkFramebuffer framebuffer);
			VkRenderPass getRenderPass(const RenderPassDescriptionVk &renderPassDescription);
			const VkPhysicalDeviceProperties &getDeviceProperties() const;

		private:
			VkInstance m_instance;
			VkDevice m_device;
			VkPhysicalDevice m_physicalDevice;
			VkPhysicalDeviceFeatures m_features;
			VkPhysicalDeviceFeatures m_enabledFeatures;
			VkPhysicalDeviceProperties m_properties;
			VkPhysicalDeviceSubgroupProperties m_subgroupProperties;
			QueueVk m_graphicsQueue;
			QueueVk m_computeQueue;
			QueueVk m_transferQueue;
			VkSurfaceKHR m_surface;
			VkDebugUtilsMessengerEXT m_debugUtilsMessenger;
			RenderPassCacheVk *m_renderPassCache;
			MemoryAllocatorVk *m_gpuMemoryAllocator;
			SwapChainVk2 *m_swapChain;
			DynamicObjectPool<ByteArray<sizeof(GraphicsPipelineVk)>> m_graphicsPipelineMemoryPool;
			DynamicObjectPool<ByteArray<sizeof(ComputePipelineVk)>> m_computePipelineMemoryPool;
			DynamicObjectPool<ByteArray<sizeof(CommandListPoolVk)>> m_commandListPoolMemoryPool;
			DynamicObjectPool<ByteArray<sizeof(ImageVk)>> m_imageMemoryPool;
			DynamicObjectPool<ByteArray<sizeof(BufferVk)>> m_bufferMemoryPool;
			DynamicObjectPool<ByteArray<sizeof(ImageViewVk)>> m_imageViewMemoryPool;
			DynamicObjectPool<ByteArray<sizeof(BufferViewVk)>> m_bufferViewMemoryPool;
			DynamicObjectPool<ByteArray<sizeof(SamplerVk)>> m_samplerMemoryPool;
			DynamicObjectPool<ByteArray<sizeof(SemaphoreVk)>> m_semaphoreMemoryPool;
			DynamicObjectPool<ByteArray<sizeof(QueryPoolVk)>> m_queryPoolMemoryPool;
			DynamicObjectPool<ByteArray<sizeof(DescriptorPoolVk)>> m_descriptorPoolMemoryPool;
		};
	}
}