#pragma once
#include <functional>
#include <vector>
#include "volk.h"
#include "VKPipeline.h"
#include "VKContext.h"

namespace VEngine
{
	typedef struct ImageViewHandle_t* ImageViewHandle;
	typedef struct ImageHandle_t* ImageHandle;
	typedef struct BufferViewHandle_t* BufferViewHandle;
	typedef struct BufferHandle_t* BufferHandle;
	typedef struct ResourceViewHandle_t* ResourceViewHandle;
	typedef struct ResourceHandle_t* ResourceHandle;

	enum class ResourceUsageType
	{
		READ_DEPTH_STENCIL,
		READ_ATTACHMENT,
		READ_TEXTURE,
		READ_STORAGE_IMAGE,
		READ_STORAGE_BUFFER,
		READ_UNIFORM_BUFFER,
		READ_VERTEX_BUFFER,
		READ_INDEX_BUFFER,
		READ_INDIRECT_BUFFER,
		READ_BUFFER_TRANSFER,
		READ_IMAGE_TRANSFER,
		READ_WRITE_STORAGE_IMAGE,
		READ_WRITE_STORAGE_BUFFER,
		READ_WRITE_ATTACHMENT,
		WRITE_DEPTH_STENCIL,
		WRITE_ATTACHMENT,
		WRITE_STORAGE_IMAGE,
		WRITE_BUFFER_TRANSFER,
		WRITE_IMAGE_TRANSFER
	};

	enum class QueueType
	{
		GRAPHICS, COMPUTE, TRANSFER,
	};

	union ClearValue
	{
		VkClearValue m_imageClearValue;
		uint32_t m_bufferClearValue;
	};

	struct ResourceUsageDescription
	{
		ResourceViewHandle m_handle;
		ResourceUsageType m_usageType;
		VkPipelineStageFlags m_stageMask;
		VkImageLayout m_finalLayout;
		bool m_clear;
		ClearValue m_clearValue;
	};

	struct ImageViewDescription
	{
		const char *m_name = "";
		ImageHandle m_imageHandle;
		VkImageViewType m_viewType;
		VkFormat m_format;
		VkComponentMapping m_components;
		VkImageSubresourceRange m_subresourceRange;
	};

	struct ImageDescription
	{
		const char *m_name = "";
		bool m_clear = false;
		ClearValue m_clearValue;
		uint32_t m_width = 1;
		uint32_t m_height = 1;
		uint32_t m_depth = 1;
		uint32_t m_layers = 1;
		uint32_t m_levels = 1;
		uint32_t m_samples = 1;
		VkImageType m_imageType = VK_IMAGE_TYPE_2D;
		VkFormat m_format = VK_FORMAT_UNDEFINED;
	};

	struct BufferViewDescription
	{
		const char *m_name = "";
		BufferHandle m_bufferHandle;
		VkFormat m_format = VK_FORMAT_UNDEFINED;
		VkDeviceSize m_offset;
		VkDeviceSize m_range;
	};

	struct BufferDescription
	{
		const char *m_name = "";
		bool m_clear = false;
		ClearValue m_clearValue;
		VkDeviceSize m_size = 0;
	};

	struct PassCreateInfo
	{
		const char *m_name;
		QueueType m_queueType;
		uint32_t m_resourceUsageCount;
		const ResourceUsageDescription *m_resourceUsages;
	};

	class Registry
	{
	public:
		ResourceHandle getResourceHandle(ImageHandle handle);
		ResourceHandle getResourceHandle(ImageViewHandle handle);
		ResourceHandle getResourceHandle(BufferHandle handle);
		ResourceHandle getResourceHandle(BufferViewHandle handle);
		VkImage getImage(ImageHandle handle) const;
		VkImage getImage(ImageViewHandle handle) const;
		VkImageView getImageView(ImageViewHandle handle) const;
		VkDescriptorImageInfo getImageInfo(ImageHandle handle, VkSampler sampler = VK_NULL_HANDLE) const;
		VkDescriptorImageInfo getImageInfo(ImageViewHandle handle, VkSampler sampler = VK_NULL_HANDLE) const;
		VkBuffer getBuffer(BufferHandle handle) const;
		VkBuffer getBuffer(BufferViewHandle handle) const;
		VkDescriptorBufferInfo getBufferInfo(BufferHandle handle) const;
		VkDescriptorBufferInfo getBufferInfo(BufferViewHandle handle) const;
		VKAllocationHandle getAllocation(ResourceHandle handle) const;
		bool firstUse(ResourceHandle handle) const;
		bool lastUse(ResourceHandle handle) const;
		void *mapMemory(BufferHandle handle) const;
		void *mapMemory(BufferViewHandle handle) const;
		void unmapMemory(BufferHandle handle) const;
		void unmapMemory(BufferViewHandle handle) const;
	};

	typedef std::function<void(VkCommandBuffer cmdBuf, const Registry &registry)> RecordFunc;

	class RenderGraph
	{
	public:
		explicit RenderGraph();
		RenderGraph(const RenderGraph &) = delete;
		RenderGraph(const RenderGraph &&) = delete;
		RenderGraph &operator= (const RenderGraph &) = delete;
		RenderGraph &operator= (const RenderGraph &&) = delete;
		~RenderGraph();
		void addPass(const char *name, QueueType queueType, uint32_t passResourceUsageCount, const ResourceUsageDescription *passResourceUsages, const RecordFunc &recordFunc);
		ImageViewHandle createImageView(const ImageViewDescription &viewDesc);
		BufferViewHandle createBufferView(const BufferViewDescription &viewDesc);
		ImageHandle createImage(const ImageDescription &imageDesc);
		BufferHandle createBuffer(const BufferDescription &bufferDesc);
		ImageHandle importImage(const ImageDescription &imageDescription, VkImage image, VkImageView imageView, VkImageLayout *layouts, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore);
		BufferHandle importBuffer(const BufferDescription &bufferDescription, VkBuffer buffer, VkDeviceSize offset, VKAllocationHandle allocation, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore);
		void reset();

	private:
		struct ResourceDescription
		{
			const char *m_name = "";
			bool m_clear = false;
			ClearValue m_clearValue;
			uint32_t m_width = 1;
			uint32_t m_height = 1;
			uint32_t m_depth = 1;
			uint32_t m_layers = 1;
			uint32_t m_levels = 1;
			uint32_t m_samples = 1;
			VkImageType m_imageType = VK_IMAGE_TYPE_2D;
			VkFormat m_format = VK_FORMAT_UNDEFINED;
			VkDeviceSize m_size = 0;
			VkImageCreateFlags m_imageFlags = 0;
			uint32_t m_subresourceCount = 0;
			uint16_t m_externalInfoIndex = 0;
			bool m_external = false;
			bool m_image = false;
		};

		struct ResourceViewDescription
		{
			const char *m_name = "";
			ResourceHandle m_resourceHandle;
			VkImageViewType m_viewType;
			VkFormat m_format;
			VkComponentMapping m_components;
			VkImageSubresourceRange m_subresourceRange;
			VkDeviceSize m_offset; // for buffers
			VkDeviceSize m_range; // for buffers
			bool m_image;
		};

		union ImageBuffer
		{
			VkImage image;
			VkBuffer buffer;
		};

		union ImageBufferView
		{
			VkImageView imageView;
			VkBufferView bufferView;
		};

		struct ResourceUsage
		{
			uint16_t m_passHandle;
			ResourceUsageType m_usageType;
			VkPipelineStageFlags m_stageMask;
			VkImageLayout m_finalLayout;
		};

		struct PassSubresources
		{
			uint32_t m_subresourcesOffset;
			uint16_t m_readSubresourceCount;
			uint16_t m_writeSubresourceCount;
		};

		struct PassRecordInfo
		{
			RecordFunc m_recordFunc;
			VkQueue m_queue;
			VkPipelineStageFlags m_eventSrcStages;
			VkPipelineStageFlags m_waitStages;
			VkPipelineStageFlags m_releaseStages;
			VkPipelineStageFlags m_endStages;
			VkAccessFlags m_memoryBarrierSrcAccessMask;
			VkAccessFlags m_memoryBarrierDstAccessMask;
			uint8_t m_waitImageBarrierCount;
			uint8_t m_waitBufferBarrierCount;
			uint8_t m_releaseImageBarrierCount;
			uint8_t m_releaseBufferBarrierCount;
			std::vector<VkEvent> m_waitEvents;
			std::vector<VkImageMemoryBarrier> m_imageBarriers;
			std::vector<VkBufferMemoryBarrier> m_bufferBarriers;
			VkEvent m_endEvent;
		};

		struct Batch
		{
			uint16_t m_passIndexOffset;
			uint16_t m_passIndexCount;
			VkQueue m_queue;
			std::vector<VkSemaphore> m_waitSemaphores;
			std::vector<VkPipelineStageFlags> m_waitDstStageMasks;
			std::vector<VkSemaphore> m_signalSemaphores;
			VkFence m_fence;
		};

		VkQueue m_queues[3];
		uint32_t m_queueFamilyIndices[3];

		std::vector<uint16_t> m_passSubresourceIndices;
		std::vector<PassSubresources> m_passSubresources;

		std::vector<ResourceDescription> m_resourceDescriptions;
		std::vector<std::vector<ResourceUsage>> m_resourceUsages; // m_resourceUsages[subresourceIndex][usageIndex]
		std::vector<uint32_t> m_resourceUsageOffsets;
		std::vector<std::pair<uint16_t, uint16_t>> m_resourceLifetimes;
		std::vector<bool> m_culledResources;

		std::vector<ResourceViewDescription> m_viewDescriptions;
		std::vector<std::vector<uint16_t>> m_viewUsages; // m_viewUsages[passIndex][viewIndex]
		std::vector<bool> m_culledViews;
		
		// allocated resources
		std::vector<VKAllocationHandle> m_allocations;
		std::vector<ImageBuffer> m_imageBuffers;
		std::vector<ImageBufferView> m_imageBufferViews;
		std::vector<VkSemaphore> m_semaphores;
		
		// data used for recording
		std::vector<uint16_t> m_passIndices;
		std::vector<VkCommandBuffer> m_commandBuffers;
		std::vector<Batch> m_batches;
		std::vector<PassRecordInfo> m_passRecordInfo;

		enum class RWAccessType
		{
			READ, WRITE, READ_WRITE
		};

		uint32_t getQueueIndex(VkQueue queue);
		VkQueue getQueue(QueueType queueType);
		bool isImageResource(ResourceUsageType usageType);
		VkPipelineStageFlags getStageMask(ResourceUsageType usageType, VkPipelineStageFlags flags);
		VkAccessFlags getAccessMask(ResourceUsageType usageType);
		VkImageLayout getImageLayout(ResourceUsageType usageType);
		RWAccessType getRWAccessType(ResourceUsageType usageType);
		VkFlags getUsageFlags(ResourceUsageType usageType);
		void forEachSubresource(ResourceViewHandle handle, std::function<void(uint32_t)> func);
		void createPasses(ResourceViewHandle finalResourceHandle);
		void createResources();
		void createSynchronization();
		void record();
	};
}