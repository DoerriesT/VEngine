#pragma once
#include <unordered_map>
#include <functional>
#include <vulkan/vulkan.h>
#include <cassert>
#include <algorithm>
#include <cstdint>
#include "Graphics/Vulkan/VKImageData.h"
#include "Graphics/Vulkan/VKBufferData.h"
#include "Graphics/Vulkan/VKResourceManager.h"

namespace VEngine
{
	namespace FrameGraph
	{
		typedef struct ImageHandle_t* ImageHandle;
		typedef struct BufferHandle_t* BufferHandle;

		enum class QueueType
		{
			GRAPHICS = 1 << 0,
			COMPUTE = 1 << 1,
			TRANSFER = 1 << 2
		};

		enum class PassType
		{
			GRAPHICS,
			COMPUTE,
			BLIT
		};

		enum class ImageInitialState
		{
			UNDEFINED, CLEAR
		};

		enum class ImageAccessType
		{
			DEPTH_STENCIL,
			TEXTURE,
			TEXTURE_LOCAL,
			STORAGE_IMAGE,
			BLIT
		};

		enum class BufferAccessType
		{
			STORAGE_BUFFER,
			UNIFORM_BUFFER,
			VERTEX_BUFFER,
			INDEX_BUFFER,
			INDIRECT_BUFFER
		};

		struct ImageDesc
		{
			const char *m_name;
			ImageInitialState m_initialState = ImageInitialState::UNDEFINED;
			uint32_t m_width;
			uint32_t m_height;
			uint32_t m_layers = 1;
			uint32_t m_levels = 1;
			uint32_t m_samples = 1;
			VkFormat m_format;
		};

		struct BufferDesc
		{
			const char *m_name;
			VkDeviceSize m_size = 0;
			VkMemoryPropertyFlags m_propertyFlags;
		};

		struct ImageResourceStage
		{
			uint32_t m_passIndex;
			ImageAccessType m_accessType;
			bool m_write;
			VkImageUsageFlags m_usage;
			VkPipelineStageFlags m_stageMask;
			VkAccessFlags m_accessMask;
			VkImageLayout m_layout;
		};

		struct BufferResourceStage
		{
			uint32_t m_passIndex;
			BufferAccessType m_accessType;
			bool m_write;
			VkBufferUsageFlags m_usage;
			VkPipelineStageFlags m_stageMask;
			VkAccessFlags m_accessMask;
		};



		struct VirtualImage
		{
			ImageDesc m_desc;
			uint32_t m_queues = 0;
			size_t m_refCount = 0;
			size_t m_resourceIndex = 0;
			std::vector<ImageResourceStage> m_stages;
		};

		struct VirtualBuffer
		{
			BufferDesc m_desc;
			uint32_t m_queues = 0;
			size_t m_refCount = 0;
			size_t m_resourceIndex = 0;
			std::vector<BufferResourceStage> m_stages;
		};

		struct ExecutionBarrier
		{
			VkPipelineStageFlags m_srcStageMask;
			VkPipelineStageFlags m_dstStageMask;
		};

		struct MemoryBarrier
		{
			VkPipelineStageFlags m_srcStageMask;
			VkPipelineStageFlags m_dstStageMask;
			VkAccessFlags m_srcAccessMask;
			VkAccessFlags m_dstAccessMask;
		};

		struct BufferBarrier
		{
			VkPipelineStageFlags m_srcStageMask;
			VkPipelineStageFlags m_dstStageMask;
			VkAccessFlags m_srcAccessMask;
			VkAccessFlags m_dstAccessMask;
			uint32_t m_srcQueueFamilyIndex;
			uint32_t m_dstQueueFamilyIndex;
			VkBuffer m_buffer;
			VkDeviceSize m_offset;
			VkDeviceSize m_size;
		};

		struct ImageBarrier
		{
			VkPipelineStageFlags m_srcStageMask;
			VkPipelineStageFlags m_dstStageMask;
			VkAccessFlags m_srcAccessMask;
			VkAccessFlags m_dstAccessMask;
			VkImageLayout m_oldLayout;
			VkImageLayout m_newLayout;
			uint32_t m_srcQueueFamilyIndex;
			uint32_t m_dstQueueFamilyIndex;
			VkImage m_image;
			VkImageSubresourceRange m_subresourceRange;
			bool m_renderPassInternal = false;
		};

		struct PipelineBarrier
		{
			VkPipelineStageFlags m_srcStageMask;
			VkPipelineStageFlags m_dstStageMask;
			VkDependencyFlags m_dependencyFlags;
			VkMemoryBarrier m_memoryBarrier;
			std::vector<VkBufferMemoryBarrier> m_bufferBarriers;
			std::vector<VkImageMemoryBarrier> m_imageBarriers;
		};

		struct WaitEvents
		{
			std::vector<VkEvent> m_events;
			VkPipelineStageFlags m_srcStageMask;
			VkPipelineStageFlags m_dstStageMask;
			VkMemoryBarrier m_memoryBarrier;
			std::vector<VkBufferMemoryBarrier> m_bufferBarriers;
			std::vector<VkImageMemoryBarrier> m_imageBarriers;
		};

		class ResourceRegistry;
		class Graph;

		struct Pass
		{
			const char *m_name;
			PassType m_passType = PassType::GRAPHICS;
			QueueType m_queue = QueueType::GRAPHICS;
			uint32_t m_width = 0;
			uint32_t m_height = 0;
			std::function<void(VkCommandBuffer, const ResourceRegistry&)> m_recordCommands;
			ImageHandle m_depthStencilAttachment;
			std::vector<ImageHandle> m_colorAttachments;
			std::vector<ImageHandle> m_readImages;
			std::vector<ImageHandle> m_writeImages;
			std::vector<BufferHandle> m_readBuffers;
			std::vector<BufferHandle> m_writeBuffers;
			size_t m_refCount = 0;
			std::vector<std::pair<size_t, ExecutionBarrier>> m_executionBarriers;
			std::vector<std::pair<size_t, MemoryBarrier>> m_memoryBarriers;
			std::vector<std::pair<size_t, BufferBarrier>> m_bufferBarriers;
			std::vector<std::pair<size_t, ImageBarrier>> m_imageBarriers;
			std::vector<VkSemaphore> m_semaphores;
			PipelineBarrier m_pipelineBarrier;
			PipelineBarrier m_endPipelineBarrier;
			WaitEvents m_waitEvents;
			VkPipelineStageFlags m_eventStageMask = 0;
			VkEvent m_event = VK_NULL_HANDLE;
			VkSemaphore m_semaphore = VK_NULL_HANDLE;
			VkRenderPass m_renderPassHandle = VK_NULL_HANDLE;
			VkFramebuffer m_framebufferHandle = VK_NULL_HANDLE;
		};

		class PassBuilder
		{
			friend class Graph;
		public:
			explicit PassBuilder(Graph &graph, Pass &pass, uint32_t passIndex);
			void setDimensions(uint32_t width, uint32_t height);
			void readDepthStencil(ImageHandle handle);
			void readInputAttachment(ImageHandle handle, uint32_t index);
			void readTexture(ImageHandle handle, VkPipelineStageFlags stageFlags, bool local = false, uint32_t baseMipLevel = 0, uint32_t levelCount = 1);
			void readStorageImage(ImageHandle handle, VkPipelineStageFlags stageFlags, bool local = false);
			void readStorageBuffer(BufferHandle handle, VkPipelineStageFlags stageFlags);
			void readUniformBuffer(BufferHandle handle, VkPipelineStageFlags stageFlags);
			void readVertexBuffer(BufferHandle handle);
			void readIndexBuffer(BufferHandle handle);
			void readIndirectBuffer(BufferHandle handle);
			void writeDepthStencil(ImageHandle handle);
			void writeColorAttachment(ImageHandle handle, uint32_t index, bool read = false);
			void writeStorageImage(ImageHandle handle, VkPipelineStageFlags stageFlags);
			void writeStorageBuffer(BufferHandle handle, VkPipelineStageFlags stageFlags);
			ImageHandle createImage(const ImageDesc &imageDesc);
			BufferHandle createBuffer(const BufferDesc &bufferDesc);

		private:
			Graph & m_graph;
			Pass &m_pass;
			uint32_t m_passIndex;
		};

		class ResourceRegistry
		{
			friend Graph;
		public:
			VKImageData getImageData(ImageHandle handle) const;
			VKBufferData getBufferData(BufferHandle handle) const;

		private:
			std::vector<VirtualImage> m_virtualImages;
			std::vector<VirtualBuffer> m_virtualBuffers;
			std::vector<VKImageData> m_images;
			std::vector<VKBufferData> m_buffers;
		};

		class Graph
		{
		public:
			void addGraphicsPass(const char *name, const std::function<std::function<void(VkCommandBuffer, const ResourceRegistry&)>(PassBuilder &)> &setup);
			void addComputePass(const char *name, QueueType queue, const std::function<std::function<void(VkCommandBuffer, const ResourceRegistry&)>(PassBuilder &)> &setup);
			void addBlitPass(const char *name, QueueType queue, ImageHandle srcHandle, ImageHandle dstHandle, const std::vector<VkImageBlit> &regions, VkFilter filter);
			void addResourceStage(ImageHandle handle, const ImageResourceStage &imageResourceStage);
			void addResourceStage(BufferHandle handle, const BufferResourceStage &bufferResourceStage);
			void setBackBuffer(ImageHandle handle);
			void setPresentParams(VkSwapchainKHR *swapChain, uint32_t swapChainImageIndex, QueueType queue, VkSemaphore waitSemaphore);
			void compile();
			void execute();
			ImageHandle createImage(const ImageDesc &imageDesc);
			BufferHandle createBuffer(const BufferDesc &bufferDesc);

		private:
			ResourceRegistry m_resourceRegistry;
			VKResourceManager m_resourceManager;
			std::vector<Pass> m_passes;
			ImageHandle m_backBufferHandle;
			VkSwapchainKHR *m_swapChain;
			uint32_t m_swapChainImageIndex;
			QueueType m_presentQueue;
			VkSemaphore m_backBufferWaitSemaphore = VK_NULL_HANDLE;

			void cull();
			void createResources();
			void createVirtualBarriers();
			void createRenderPasses();
			void createPhysicalBarriers();
			uint32_t queueIndexFromQueueType(QueueType queueType);
		};
	}

}