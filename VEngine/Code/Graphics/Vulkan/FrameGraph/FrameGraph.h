#pragma once
#include <unordered_map>
#include <functional>
#include "Graphics/Vulkan/volk.h"
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <variant>
#include <bitset>
#include "Graphics/Vulkan/VKSyncPrimitiveAllocator.h"
#include "Graphics/Vulkan/VKMemoryAllocator.h"
#include "Graphics/Vulkan/VKPipeline.h"

namespace VEngine
{
	namespace FrameGraph
	{
		typedef struct ImageHandle_t* ImageHandle;
		typedef struct BufferHandle_t* BufferHandle;
		typedef struct ResourceHandle_t* ResourceHandle;

		class PassBuilder;
		class ResourceRegistry;

		class Pass
		{
		public:
			virtual void record(VkCommandBuffer cmdBuf, const ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass) = 0;
		};

		enum class QueueType
		{
			NONE = 0,
			GRAPHICS = 1 << 0,
			COMPUTE = 1 << 1,
			TRANSFER = 1 << 2,
		};

		enum class PassType
		{
			GRAPHICS,
			COMPUTE,
			GENERIC,
			HOST_ACCESS,
			CLEAR
		};

		union ClearValue
		{
			VkClearValue m_imageClearValue;
			uint32_t m_bufferClearValue;
		};

		struct ImageDescription
		{
			const char *m_name = "";
			bool m_concurrent = false;
			bool m_clear = false;
			ClearValue m_clearValue;
			uint32_t m_width = 0;
			uint32_t m_height = 0;
			uint32_t m_layers = 1;
			uint32_t m_levels = 1;
			uint32_t m_samples = 1;
			VkFormat m_format = VK_FORMAT_UNDEFINED;
		};

		struct BufferDescription
		{
			const char *m_name = "";
			bool m_concurrent = false;
			bool m_clear = false;
			ClearValue m_clearValue;
			VkDeviceSize m_size = 0;
			bool m_hostVisible = false;
		};

		struct PassTimingInfo
		{
			const char *m_passName;
			float m_passTime;
			float m_passTimeWithSync;
		};

		class Graph;

		class PassBuilder
		{
		public:
			explicit PassBuilder(Graph &graph, size_t passIndex);
			void readDepthStencil(ImageHandle handle);
			void readInputAttachment(ImageHandle handle, uint32_t index);
			void readTexture(ImageHandle handle, VkPipelineStageFlags stageFlags);
			void readStorageImage(ImageHandle handle, VkPipelineStageFlags stageFlags);
			void readStorageBuffer(BufferHandle handle, VkPipelineStageFlags stageFlags);
			void readUniformBuffer(BufferHandle handle, VkPipelineStageFlags stageFlags);
			void readVertexBuffer(BufferHandle handle);
			void readIndexBuffer(BufferHandle handle);
			void readIndirectBuffer(BufferHandle handle);
			void readImageTransfer(ImageHandle handle);
			void readWriteStorageImage(ImageHandle handle, VkPipelineStageFlags stageFlags);
			void readWriteStorageBuffer(BufferHandle handle, VkPipelineStageFlags stageFlags);
			void writeDepthStencil(ImageHandle handle);
			void writeColorAttachment(ImageHandle handle, uint32_t index);
			void writeStorageImage(ImageHandle handle, VkPipelineStageFlags stageFlags);
			void writeStorageBuffer(BufferHandle handle, VkPipelineStageFlags stageFlags);
			void writeImageTransfer(ImageHandle handle);
			void writeBufferFromHost(BufferHandle handle);
			ImageHandle createImage(const ImageDescription &imageDesc);
			BufferHandle createBuffer(const BufferDescription &bufferDesc);

		private:
			Graph &m_graph;
			size_t m_passIndex;
		};

		class ResourceRegistry
		{
		public:
			explicit ResourceRegistry(Graph &graph, size_t passIndex);
			VkImage getImage(ImageHandle handle) const;
			VkImageView getImageView(ImageHandle handle) const;
			VkDescriptorImageInfo getImageInfo(ImageHandle handle) const;
			VkBuffer getBuffer(BufferHandle handle) const;
			VkDescriptorBufferInfo getBufferInfo(BufferHandle handle) const;
			const VKAllocationHandle &getAllocation(ResourceHandle handle) const;
			const VKAllocationHandle &getAllocation(ImageHandle handle) const;
			const VKAllocationHandle &getAllocation(BufferHandle handle) const;
			void *mapMemory(BufferHandle handle) const;
			void unmapMemory(BufferHandle handle) const;

		private:
			Graph &m_graph;
			size_t m_passIndex;
		};

		class Graph
		{
			friend class PassBuilder;
			friend class ResourceRegistry;
		public:
			typedef std::function<void(VkCommandBuffer cmdBuf, const ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)> RecordFunc;

			explicit Graph(VKSyncPrimitiveAllocator &syncPrimitiveAllocator);
			Graph(const Graph &) = delete;
			Graph(const Graph &&) = delete;
			Graph &operator= (const Graph &) = delete;
			Graph &operator= (const Graph &&) = delete;
			~Graph();
			void addGraphicsPass(const char *name, uint32_t width, uint32_t height, std::function<void(PassBuilder)> setupFunc, RecordFunc recordFunc);
			void addComputePass(const char *name, QueueType queueType, std::function<void(PassBuilder)> setupFunc, RecordFunc recordFunc);
			void addGenericPass(const char *name, QueueType queueType, std::function<void(PassBuilder)> setupFunc, RecordFunc recordFunc);
			void addHostAccessPass(const char *name, std::function<void(PassBuilder)> setupFunc, RecordFunc recordFunc);

			void execute(ResourceHandle finalResourceHandle, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED);
			void reset();
			ImageHandle createImage(const ImageDescription &imageDescription);
			BufferHandle createBuffer(const BufferDescription &bufferDescription);
			ImageHandle importImage(const ImageDescription &imageDescription, VkImage image, VkImageView imageView, VkImageLayout *layout, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore);
			BufferHandle importBuffer(const BufferDescription &bufferDescription, VkBuffer buffer, VKAllocationHandle allocation, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore);
			void getTimingInfo(size_t &count, PassTimingInfo *data);

		private:
			enum
			{
				MAX_RESOURCES = 128,
				MAX_PASSES = 64,
				MAX_COLOR_ATTACHMENTS = 8,
				MAX_WAIT_SEMAPHORES = 8,
				MAX_SIGNAL_SEMAPHORES = 8,
				MAX_WAIT_EVENTS = 8,
				MAX_SIGNAL_EVENTS = 8,
				MAX_RESOURCE_BARRIERS = 16,
			};

			struct ResourceDescription
			{
				uint32_t m_width = 0;
				uint32_t m_height = 0;
				uint32_t m_layers = 1;
				uint32_t m_levels = 1;
				uint32_t m_samples = 1;
				VkFormat m_format = VK_FORMAT_UNDEFINED;
				VkDeviceSize m_size = 0;
				bool m_hostVisible = false;
			};

			struct FramebufferInfo
			{
				uint32_t m_width = 0;
				uint32_t m_height = 0;
				ImageHandle m_colorAttachments[MAX_COLOR_ATTACHMENTS] = {};
				ImageHandle m_depthStencilAttachment = 0;
			};

			struct ResourceUsage
			{
				VkPipelineStageFlags m_stageMask;
				VkAccessFlags m_accessMask;
				VkFlags m_usageFlags; // either VkBufferUsageFlags or VkImageUsageFlags
				VkImageLayout m_imageLayout;
			};

			struct SyncBits
			{
				uint32_t m_waitSemaphoreCount;
				uint32_t m_signalSemaphoreCount;
				uint32_t m_waitEventCount;
				uint32_t m_releaseCount;
				uint32_t m_resourceBarrierCount;
				std::bitset<MAX_PASSES + MAX_RESOURCES * 2> m_waitSemaphores;
				std::bitset<MAX_PASSES + MAX_RESOURCES * 2> m_signalSemaphores;
				std::bitset<MAX_PASSES> m_waitEvents;
				std::bitset<MAX_RESOURCES> m_releaseResources;
				std::bitset<MAX_RESOURCES> m_barrierResources;
				VkPipelineStageFlags m_waitEventsSrcStageMask = 0;
				VkAccessFlags m_memoryBarrierSrcAccessMask = 0;
				VkAccessFlags m_memoryBarrierDstAccessMask = 0;
			};

			struct TimestampQueryIndices
			{
				uint32_t m_beforeSync;
				uint32_t m_beforePass;
				uint32_t m_afterPass;
				uint32_t m_afterSync;
			};

			VKSyncPrimitiveAllocator &m_syncPrimitiveAllocator;
			const char *m_resourceNames[MAX_RESOURCES];
			const char *m_passNames[MAX_PASSES];
			VkImageLayout *m_externalLayouts[MAX_RESOURCES];
			RecordFunc m_recordFunctions[MAX_PASSES];
			ClearValue m_clearValues[MAX_RESOURCES];
			PassType m_passTypes[MAX_PASSES];
			QueueType m_queueType[MAX_PASSES];
			ResourceDescription m_resourceDescriptions[MAX_RESOURCES];
			VkCommandBuffer m_commandBuffers[3][MAX_PASSES];
			bool m_recordTimings = true;
			size_t m_timingInfoCount = 0;

			///////////////////////////////////////////////////
			// everything below needs to be reset before use //
			///////////////////////////////////////////////////

			VkQueryPool m_queryPool;
			VkCommandPool m_graphicsCommandPool;
			VkCommandPool m_computeCommandPool;
			VkCommandPool m_transferCommandPool;
			size_t m_graphicsCommandBufferCount = 0;
			size_t m_computeCommandBufferCount = 0;
			size_t m_transferCommandBufferCount = 0;
			size_t m_resourceCount = 0;
			size_t m_passCount = 0;
			uint32_t m_timestampQueryCount = 0;
			// each element holds a bitset that specifies if the resource is read/written in the pass
			std::bitset<MAX_PASSES> m_writeResources[MAX_RESOURCES];
			std::bitset<MAX_PASSES> m_readResources[MAX_RESOURCES];
			std::bitset<MAX_PASSES> m_accessedResources[MAX_RESOURCES];
			std::bitset<MAX_PASSES> m_attachmentResources[MAX_RESOURCES];
			std::bitset<MAX_PASSES> m_culledPasses;
			std::bitset<MAX_RESOURCES> m_culledResources;
			std::bitset<MAX_RESOURCES> m_concurrentResources;
			std::bitset<MAX_RESOURCES> m_externalResources;
			std::bitset<MAX_RESOURCES> m_imageResources;
			std::bitset<MAX_RESOURCES> m_clearResources;
			FramebufferInfo m_framebufferInfo[MAX_PASSES];
			ResourceUsage m_resourceUsages[MAX_RESOURCES][MAX_PASSES];
			VkFence m_fence = 0;
			// (n < MAX_PASSES) holds internal semaphores
			// (n >= MAX_PASSES) even indices hold wait, odd indices signal semaphores for imported resources
			VkSemaphore m_semaphores[MAX_PASSES + MAX_RESOURCES * 2];
			VkEvent m_events[MAX_PASSES];
			VkImage m_images[MAX_RESOURCES];
			VkImageView m_imageViews[MAX_RESOURCES];
			VkBuffer m_buffers[MAX_RESOURCES];
			VKAllocationHandle m_allocations[MAX_RESOURCES];
			std::pair<VkRenderPass, VkFramebuffer> m_renderpassFramebufferHandles[MAX_PASSES];
			VkPipelineStageFlags m_passStageMasks[MAX_PASSES];
			PassTimingInfo m_timingInfos[MAX_PASSES];

			void cull(size_t *firstResourceUses, size_t *lastResourceUses, ResourceHandle finalResourceHandle);
			void createClearPasses(size_t *firstResourceUses);
			void createResources();
			void createRenderPasses(size_t *firstResourceUses, size_t *lastResourceUses, ResourceHandle finalResourceHandle);
			void createSynchronization(size_t *firstResourceUses, size_t *lastResourceUses, SyncBits *syncBits);
			void recordAndSubmit(size_t *firstResourceUses, size_t *lastResourceUses, SyncBits *syncBits, ResourceHandle finalResourceHandle, VkImageLayout finalLayout);
			uint32_t queueIndexFromQueueType(QueueType queueType);
			void createRenderPassDescription(size_t passIndex, VKRenderPassDescription &renderPassDescription);
		};
	}
}