#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "QueueVk.h"
#include "Utility/ObjectPool.h"
#include "PipelineVk.h"
#include "CommandListPoolVk.h"
#include "ResourceVk.h"
#include "SemaphoreVk.h"
#include "QueryPoolVk.h"
#include "DescriptorSetVk.h"

namespace VEngine
{
	namespace gal
	{
		class SwapChainVk;
		class RenderPassCacheVk;
		class FramebufferCacheVk;
		class MemoryAllocatorVk;
		struct RenderPassDescriptionVk;
		struct FramebufferDescriptionVk;

		class GraphicsDeviceVk : public GraphicsDevice
		{
		public:
			explicit GraphicsDeviceVk(void *windowHandle, bool debugLayer);
			GraphicsDeviceVk(GraphicsDeviceVk &) = delete;
			GraphicsDeviceVk(GraphicsDeviceVk &&) = delete;
			GraphicsDeviceVk &operator= (const GraphicsDeviceVk &) = delete;
			GraphicsDeviceVk &operator= (const GraphicsDeviceVk &&) = delete;
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
			void createImageView(const ImageViewCreateInfo &imageViewCreateInfo, ImageView **imageView) override;
			void createImageView(Image *image, ImageView **imageView) override;
			void createBufferView(const BufferViewCreateInfo &bufferViewCreateInfo, BufferView **bufferView) override;
			void destroyImageView(ImageView *imageView) override;
			void destroyBufferView(BufferView *bufferView) override;
			void createSampler(const SamplerCreateInfo &samplerCreateInfo, Sampler **sampler) override;
			void destroySampler(Sampler *sampler) override;
			void createSemaphore(uint64_t initialValue, Semaphore **semaphore) override;
			void destroySemaphore(Semaphore *semaphore) override;
			void createDescriptorPool(uint32_t maxSets, const uint32_t typeCounts[(size_t)DescriptorType::RANGE_SIZE], DescriptorPool **descriptorPool) override;
			void destroyDescriptorPool(DescriptorPool *descriptorPool) override;
			void createDescriptorSetLayout(uint32_t bindingCount, const DescriptorSetLayoutBinding *bindings, DescriptorSetLayout **descriptorSetLayout) override;
			void destroyDescriptorSetLayout(DescriptorSetLayout *descriptorSetLayout) override;
			void createSwapChain(const Queue *presentQueue, uint32_t width, uint32_t height, SwapChain **swapChain) override;
			void destroySwapChain() override;
			void waitIdle() override;
			void setDebugObjectName(ObjectType objectType, void *object, const char *name) override;
			Queue *getGraphicsQueue() override;
			Queue *getComputeQueue() override;
			Queue *getTransferQueue() override;
			void getQueryPoolResults(QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void *data, uint64_t stride, QueryResultFlags flags) override;
			float getTimestampPeriod() const override;
			uint64_t getMinUniformBufferOffsetAlignment() const;
			uint64_t getMinStorageBufferOffsetAlignment() const;
			float getMaxSamplerAnisotropy() const;
			VkDevice getDevice() const;
			VkRenderPass getRenderPass(const RenderPassDescriptionVk &renderPassDescription);
			VkFramebuffer getFramebuffer(const FramebufferDescriptionVk &framebufferDescription);
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
			FramebufferCacheVk *m_framebufferCache;
			MemoryAllocatorVk *m_gpuMemoryAllocator;
			SwapChainVk *m_swapChain;
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
			DynamicObjectPool<ByteArray<sizeof(DescriptorSetLayoutVk)>> m_descriptorSetLayoutMemoryPool;
			bool m_debugLayers;
		};
	}
}