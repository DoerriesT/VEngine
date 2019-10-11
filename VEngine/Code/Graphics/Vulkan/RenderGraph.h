#pragma once
#include <functional>
#include <vector>
#include "volk.h"
#include "VKPipeline.h"
#include "VKContext.h"
#include "CommandBufferPool.h"
#include "Graphics/PassTimingInfo.h"

namespace VEngine
{
	typedef struct ImageViewHandle_t* ImageViewHandle;
	typedef struct ImageHandle_t* ImageHandle;
	typedef struct BufferViewHandle_t* BufferViewHandle;
	typedef struct BufferHandle_t* BufferHandle;
	typedef struct ResourceViewHandle_t* ResourceViewHandle;
	typedef struct ResourceHandle_t* ResourceHandle;

	class RenderGraph;

	enum class ResourceState
	{
		UNDEFINED,
		READ_DEPTH_STENCIL,
		READ_ATTACHMENT,
		READ_TEXTURE_VERTEX_SHADER,
		READ_TEXTURE_TESSC_SHADER,
		READ_TEXTURE_TESSE_SHADER,
		READ_TEXTURE_GEOMETRY_SHADER,
		READ_TEXTURE_FRAGMENT_SHADER,
		READ_TEXTURE_COMPUTE_SHADER,
		READ_STORAGE_IMAGE_VERTEX_SHADER,
		READ_STORAGE_IMAGE_TESSC_SHADER,
		READ_STORAGE_IMAGE_TESSE_SHADER,
		READ_STORAGE_IMAGE_GEOMETRY_SHADER,
		READ_STORAGE_IMAGE_FRAGMENT_SHADER,
		READ_STORAGE_IMAGE_COMPUTE_SHADER,
		READ_STORAGE_BUFFER_VERTEX_SHADER,
		READ_STORAGE_BUFFER_TESSC_SHADER,
		READ_STORAGE_BUFFER_TESSE_SHADER,
		READ_STORAGE_BUFFER_GEOMETRY_SHADER,
		READ_STORAGE_BUFFER_FRAGMENT_SHADER,
		READ_STORAGE_BUFFER_COMPUTE_SHADER,
		READ_UNIFORM_BUFFER_VERTEX_SHADER,
		READ_UNIFORM_BUFFER_TESSC_SHADER,
		READ_UNIFORM_BUFFER_TESSE_SHADER,
		READ_UNIFORM_BUFFER_GEOMETRY_SHADER,
		READ_UNIFORM_BUFFER_FRAGMENT_SHADER,
		READ_UNIFORM_BUFFER_COMPUTE_SHADER,
		READ_VERTEX_BUFFER,
		READ_INDEX_BUFFER,
		READ_INDIRECT_BUFFER,
		READ_BUFFER_TRANSFER,
		READ_IMAGE_TRANSFER,
		READ_WRITE_STORAGE_IMAGE_VERTEX_SHADER,
		READ_WRITE_STORAGE_IMAGE_TESSC_SHADER,
		READ_WRITE_STORAGE_IMAGE_TESSE_SHADER,
		READ_WRITE_STORAGE_IMAGE_GEOMETRY_SHADER,
		READ_WRITE_STORAGE_IMAGE_FRAGMENT_SHADER,
		READ_WRITE_STORAGE_IMAGE_COMPUTE_SHADER,
		READ_WRITE_STORAGE_BUFFER_VERTEX_SHADER,
		READ_WRITE_STORAGE_BUFFER_TESSC_SHADER,
		READ_WRITE_STORAGE_BUFFER_TESSE_SHADER,
		READ_WRITE_STORAGE_BUFFER_GEOMETRY_SHADER,
		READ_WRITE_STORAGE_BUFFER_FRAGMENT_SHADER,
		READ_WRITE_STORAGE_BUFFER_COMPUTE_SHADER,
		READ_WRITE_ATTACHMENT,
		WRITE_DEPTH_STENCIL,
		WRITE_ATTACHMENT,
		WRITE_STORAGE_IMAGE_VERTEX_SHADER,
		WRITE_STORAGE_IMAGE_TESSC_SHADER,
		WRITE_STORAGE_IMAGE_TESSE_SHADER,
		WRITE_STORAGE_IMAGE_GEOMETRY_SHADER,
		WRITE_STORAGE_IMAGE_FRAGMENT_SHADER,
		WRITE_STORAGE_IMAGE_COMPUTE_SHADER,
		WRITE_STORAGE_BUFFER_VERTEX_SHADER,
		WRITE_STORAGE_BUFFER_TESSC_SHADER,
		WRITE_STORAGE_BUFFER_TESSE_SHADER,
		WRITE_STORAGE_BUFFER_GEOMETRY_SHADER,
		WRITE_STORAGE_BUFFER_FRAGMENT_SHADER,
		WRITE_STORAGE_BUFFER_COMPUTE_SHADER,
		WRITE_BUFFER_TRANSFER,
		WRITE_IMAGE_TRANSFER,
		PRESENT_IMAGE,
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
		ResourceViewHandle m_viewHandle;
		ResourceState m_resourceState;
		bool m_customUsage;
		ResourceState m_finalResourceState;
	};

	struct ImageViewDescription
	{
		const char *m_name = "";
		ImageHandle m_imageHandle;
		VkImageSubresourceRange m_subresourceRange;
		VkImageViewType m_viewType = VK_IMAGE_VIEW_TYPE_2D;
		VkFormat m_format = VK_FORMAT_UNDEFINED;
		VkComponentMapping m_components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	};

	struct ImageDescription
	{
		const char *m_name = "";
		bool m_concurrent = false;
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
		VkFlags m_usageFlags = 0;
	};

	struct BufferViewDescription
	{
		const char *m_name = "";
		BufferHandle m_bufferHandle;
		VkDeviceSize m_offset;
		VkDeviceSize m_range;
		VkFormat m_format = VK_FORMAT_UNDEFINED;
	};

	struct BufferDescription
	{
		const char *m_name = "";
		bool m_concurrent = false;
		bool m_clear = false;
		ClearValue m_clearValue;
		VkDeviceSize m_size = 0;
		VkFlags m_usageFlags = 0;
		bool m_hostVisible;
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
		explicit Registry(const RenderGraph &graph, uint16_t passHandleIndex);
		VkImage getImage(ImageHandle handle) const;
		VkImage getImage(ImageViewHandle handle) const;
		VkImageView getImageView(ImageViewHandle handle) const;
		VkDescriptorImageInfo getImageInfo(ImageViewHandle handle, ResourceState state, VkSampler sampler = VK_NULL_HANDLE) const;
		ImageViewDescription getImageViewDescription(ImageViewHandle handle) const;
		ImageDescription getImageDescription(ImageViewHandle handle) const;
		VkAttachmentDescription getAttachmentDescription(ImageViewHandle handle, ResourceState state, bool clear = false) const;
		VkImageLayout getLayout(ResourceState state) const;
		VkBuffer getBuffer(BufferHandle handle) const;
		VkBuffer getBuffer(BufferViewHandle handle) const;
		VkBufferView getBufferView(BufferViewHandle handle) const;
		VkDescriptorBufferInfo getBufferInfo(BufferViewHandle handle) const;
		BufferViewDescription getBufferViewDescription(BufferViewHandle handle) const;
		BufferDescription getBufferDescription(BufferViewHandle handle) const;
		bool firstUse(ResourceHandle handle) const;
		bool lastUse(ResourceHandle handle) const;
		void map(BufferViewHandle handle, void **data) const;
		void unmap(BufferViewHandle handle) const;

	private:
		const RenderGraph &m_graph;
		const uint16_t m_passHandleIndex;
	};

	typedef std::function<void(VkCommandBuffer cmdBuf, const Registry &registry)> RecordFunc;

	class RenderGraph
	{
		friend class Registry;
	public:
		static const VkQueue undefinedQueue;

		explicit RenderGraph();
		RenderGraph(const RenderGraph &) = delete;
		RenderGraph(const RenderGraph &&) = delete;
		RenderGraph &operator= (const RenderGraph &) = delete;
		RenderGraph &operator= (const RenderGraph &&) = delete;
		~RenderGraph();
		void addPass(const char *name, QueueType queueType, uint32_t passResourceUsageCount, const ResourceUsageDescription *passResourceUsages, const RecordFunc &recordFunc, bool forceExecution = false);
		ImageViewHandle createImageView(const ImageViewDescription &viewDesc);
		BufferViewHandle createBufferView(const BufferViewDescription &viewDesc);
		ImageHandle createImage(const ImageDescription &imageDesc);
		BufferHandle createBuffer(const BufferDescription &bufferDesc);
		ImageHandle importImage(const ImageDescription &imageDesc, VkImage image, VkQueue *queues = nullptr, ResourceState *resourceStates = nullptr);
		BufferHandle importBuffer(const BufferDescription &bufferDesc, VkBuffer buffer, VkDeviceSize offset, VkQueue *queue = nullptr, ResourceState *resourceState = nullptr);
		void reset();
		void execute(ResourceViewHandle finalResourceHandle, VkSemaphore waitSemaphore, uint32_t *signalSemaphoreCount, VkSemaphore **signalSemaphores, ResourceState finalResourceState = ResourceState::UNDEFINED, QueueType queueType = QueueType::GRAPHICS);
		void getTimingInfo(size_t *count, const PassTimingInfo **data) const;

	private:
		struct ResourceDescription
		{
			const char *m_name = "";
			VkFlags m_usageFlags = 0;
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
			VkDeviceSize m_offset = 0;
			VkDeviceSize m_size = 0;
			VkImageCreateFlags m_imageFlags = 0;
			uint32_t m_subresourceCount = 1;
			uint16_t m_externalInfoIndex = 0;
			bool m_concurrent = false;
			bool m_external = false;
			bool m_image = false;
			bool m_hostVisible;
		};

		struct ExternalResourceInfo
		{
			VkQueue *m_queues;
			ResourceState *m_resourceStates;
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
			ResourceState m_initialResourceState;
			ResourceState m_finalResourceState;
		};

		struct ResourceStateInfo
		{
			VkPipelineStageFlags m_stageMask;
			VkAccessFlags m_accessMask;
			VkImageLayout m_layout;
			VkFlags m_usageFlags; // either VkImageUsageFlags or VkBufferUsageFlags
			bool m_readAccess;
			bool m_writeAccess;
		};

		struct PassSubresources
		{
			uint32_t m_subresourcesOffset;
			uint16_t m_readSubresourceCount;
			uint16_t m_writeSubresourceCount;
			bool m_forceExecution;
		};

		struct PassRecordInfo
		{
			RecordFunc m_recordFunc;
			VkQueue m_queue;
			VkPipelineStageFlags m_srcStageMask;
			VkPipelineStageFlags m_dstStageMask;
			VkPipelineStageFlags m_releaseStageMask;
			VkAccessFlags m_memoryBarrierSrcAccessMask;
			VkAccessFlags m_memoryBarrierDstAccessMask;
			uint8_t m_waitImageBarrierCount;
			uint8_t m_waitBufferBarrierCount;
			uint8_t m_releaseImageBarrierCount;
			uint8_t m_releaseBufferBarrierCount;
			std::vector<VkImageMemoryBarrier> m_imageBarriers;
			std::vector<VkBufferMemoryBarrier> m_bufferBarriers;
		};

		struct Batch
		{
			uint16_t m_passIndexOffset;
			uint16_t m_passIndexCount;
			uint16_t m_cmdBufOffset;
			VkQueue m_queue;
			std::vector<VkSemaphore> m_waitSemaphores;
			std::vector<VkPipelineStageFlags> m_waitDstStageMasks;
			uint16_t m_signalSemaphoreOffset;
			uint16_t m_signalSemaphoreCount;
			VkFence m_fence;
		};

		enum { TIMESTAMP_QUERY_COUNT = 1024 };

		VkQueue m_queues[3];
		uint32_t m_queueFamilyIndices[3];
		uint64_t m_queueTimestampMasks[3];
		bool m_recordTimings = true;
		std::unique_ptr<PassTimingInfo[]> m_timingInfos;

		///////////////////////////////////////////////////
		// everything below needs to be reset before use //
		///////////////////////////////////////////////////

		uint32_t m_timingInfoCount;
		std::vector<const char *> m_passNames;
		std::vector<uint32_t> m_passSubresourceIndices;
		std::vector<PassSubresources> m_passSubresources;

		std::vector<ResourceDescription> m_resourceDescriptions;
		std::vector<ExternalResourceInfo> m_externalResourceInfo;
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
		std::vector<VkSemaphore> m_finalResourceSemaphores; // copy of some semaphores in m_semaphores: DON'T FREE THESE (double free)
		VkFence m_fences[3];
		CommandBufferPool m_commandBufferPool;
		VkQueryPool m_queryPool;

		// data used for recording
		uint32_t m_externalSemaphoreSignalCounts[3];
		VkPipelineStageFlags m_externalReleaseMasks[3];
		std::vector<VkImageMemoryBarrier> m_externalReleaseImageBarriers[3];
		std::vector<VkBufferMemoryBarrier> m_externalReleaseBufferBarriers[3];
		std::vector<uint16_t> m_passHandleOrder;
		std::vector<VkCommandBuffer> m_commandBuffers;
		std::vector<Batch> m_batches;
		std::vector<PassRecordInfo> m_passRecordInfo;

		uint32_t getQueueFamilyIndex(VkQueue queue) const;
		VkQueue getQueue(QueueType queueType) const;
		ResourceStateInfo getResourceStateInfo(ResourceState resourceState) const;
		void forEachSubresource(ResourceViewHandle handle, std::function<void(uint32_t)> func);
		void createPasses(ResourceViewHandle finalResourceHandle, ResourceState finalResourceState, QueueType queueType);
		void createResources();
		void createSynchronization(ResourceViewHandle finalResourceHandle, VkSemaphore finalResourceWaitSemaphore);
		void record();
	};
}