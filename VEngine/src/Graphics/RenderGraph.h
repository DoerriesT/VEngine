#pragma once
#include <functional>
#include <vector>
#include "PassTimingInfo.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "CommandListFramePool.h"

namespace VEngine
{
	namespace rg
	{
		typedef struct ImageViewHandle_t *ImageViewHandle;
		typedef struct ImageHandle_t *ImageHandle;
		typedef struct BufferViewHandle_t *BufferViewHandle;
		typedef struct BufferHandle_t *BufferHandle;
		typedef struct ResourceViewHandle_t *ResourceViewHandle;
		typedef struct ResourceHandle_t *ResourceHandle;

		class RenderGraph;

		enum class QueueType
		{
			GRAPHICS, COMPUTE, TRANSFER,
		};

		struct ResourceStateStageMask
		{
			gal::ResourceState m_resourceState = gal::ResourceState::UNDEFINED;
			gal::PipelineStageFlags m_stageMask = gal::PipelineStageFlagBits::TOP_OF_PIPE_BIT;
		};

		struct ResourceStateData
		{
			ResourceStateStageMask m_stateStageMask;
			gal::Queue *m_queue;
		};

		
		union ClearValue
		{
			gal::ClearValue m_imageClearValue;
			uint32_t m_bufferClearValue;
		};

		

		struct ResourceUsageDescription
		{
			ResourceViewHandle m_viewHandle;
			ResourceStateStageMask m_stateStageMask;
			bool m_customUsage;
			ResourceStateStageMask m_finalStateStageMask;
		};

		struct ImageViewDescription
		{
			const char *m_name = "";
			ImageHandle m_imageHandle;
			gal::ImageSubresourceRange m_subresourceRange;
			gal::ImageViewType m_viewType = gal::ImageViewType::_2D;
			gal::Format m_format = gal::Format::UNDEFINED;
			gal::ComponentMapping m_components = {};
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
			gal::SampleCount m_samples = gal::SampleCount::_1;
			gal::ImageType m_imageType = gal::ImageType::_2D;
			gal::Format m_format = gal::Format::UNDEFINED;
			uint32_t m_usageFlags = 0;
		};

		struct BufferViewDescription
		{
			const char *m_name = "";
			BufferHandle m_bufferHandle;
			uint64_t m_offset;
			uint64_t m_range;
			uint32_t m_structureByteStride;
			gal::Format m_format = gal::Format::UNDEFINED;
		};

		struct BufferDescription
		{
			const char *m_name = "";
			bool m_clear = false;
			ClearValue m_clearValue;
			uint64_t m_size = 0;
			uint32_t m_usageFlags = 0;
			bool m_hostVisible = false;
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
			gal::Image *getImage(ImageHandle handle) const;
			gal::Image *getImage(ImageViewHandle handle) const;
			gal::ImageView *getImageView(ImageViewHandle handle) const;
			ImageViewDescription getImageViewDescription(ImageViewHandle handle) const;
			ImageDescription getImageDescription(ImageViewHandle handle) const;
			gal::Buffer *getBuffer(BufferHandle handle) const;
			gal::Buffer *getBuffer(BufferViewHandle handle) const;
			gal::BufferView *getBufferView(BufferViewHandle handle) const;
			gal::DescriptorBufferInfo getBufferInfo(BufferViewHandle handle) const;
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

		typedef std::function<void(gal::CommandList *cmdList, const Registry &registry)> RecordFunc;

		class RenderGraph
		{
			friend class Registry;
		public:
			static const gal::Queue *undefinedQueue;

			explicit RenderGraph(gal::GraphicsDevice *graphicsDevice, gal::Semaphore **semaphores, uint64_t *semaphoreValues);
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
			ImageHandle importImage(gal::Image *image, const char *name, bool clear, const ClearValue &clearValue, ResourceStateData *resourceStateData = nullptr);
			BufferHandle importBuffer(gal::Buffer *buffer, const char *name, bool clear, const ClearValue &clearValue, ResourceStateData *resourceStateData = nullptr);
			void reset();
			void execute(ResourceViewHandle finalResourceHandle, const ResourceStateData &finalResourceStateData, bool forceWaitOnSemaphore = false, uint64_t waitValue = 0);
			void getTimingInfo(size_t *count, const PassTimingInfo **data) const;
			gal::GraphicsDevice *getGraphicsDevice();

		private:
			struct ResourceDescription
			{
				const char *m_name = "";
				uint32_t m_usageFlags = 0;
				bool m_clear = false;
				ClearValue m_clearValue;
				uint32_t m_width = 1;
				uint32_t m_height = 1;
				uint32_t m_depth = 1;
				uint32_t m_layers = 1;
				uint32_t m_levels = 1;
				gal::SampleCount m_samples = gal::SampleCount::_1;
				gal::ImageType m_imageType = gal::ImageType::_2D;
				gal::Format m_format = gal::Format::UNDEFINED;
				uint64_t m_offset = 0;
				uint64_t m_size = 0;
				gal::ImageCreateFlags m_imageFlags = 0;
				uint32_t m_subresourceCount = 1;
				uint16_t m_externalInfoIndex = 0;
				bool m_concurrent = false;
				bool m_external = false;
				bool m_image = false;
				bool m_hostVisible;
			};

			struct ResourceViewDescription
			{
				const char *m_name = "";
				ResourceHandle m_resourceHandle;
				gal::ImageViewType m_viewType;
				gal::Format m_format;
				gal::ComponentMapping m_components;
				gal::ImageSubresourceRange m_subresourceRange;
				uint64_t m_offset; // for buffers
				uint64_t m_range; // for buffers
				uint32_t m_structureByteStride; // for structured buffers
				bool m_image;
			};

			union ImageBuffer
			{
				gal::Image *image;
				gal::Buffer *buffer;
			};

			union ImageBufferView
			{
				gal::ImageView *imageView;
				gal::BufferView *bufferView;
			};

			struct ResourceUsage
			{
				uint16_t m_passHandle;
				ResourceStateStageMask m_initialResourceState;
				ResourceStateStageMask m_finalResourceState;
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
				gal::Queue *m_queue;
				uint32_t m_signalValue;
				std::vector<gal::Barrier> m_beforeBarriers;
				std::vector<gal::Barrier> m_afterBarriers;
			};

			struct Batch
			{
				uint16_t m_passIndexOffset;
				uint16_t m_passIndexCount;
				uint16_t m_cmdListOffset;
				gal::Queue *m_queue;
				gal::PipelineStageFlags m_waitDstStageMasks[3];
				uint64_t m_waitValues[3];
				uint64_t m_signalValue;
				bool m_lastBatchOnQueue;
			};

			enum { TIMESTAMP_QUERY_COUNT = 1024 };

			gal::GraphicsDevice *m_graphicsDevice;
			gal::Semaphore *m_semaphores[3];
			uint64_t *m_semaphoreValues[3];
			gal::Queue *m_queues[3];
			uint64_t m_queueTimestampMasks[3];
			bool m_recordTimings = true;
			std::unique_ptr<PassTimingInfo[]> m_timingInfos;
			gal::Buffer *m_queryResultBuffer;
			bool m_readyToRecord = true;

			///////////////////////////////////////////////////
			// everything below needs to be reset before use //
			///////////////////////////////////////////////////

			uint32_t m_timingInfoCount;
			std::vector<const char *> m_passNames;
			std::vector<uint32_t> m_passSubresourceIndices;
			std::vector<PassSubresources> m_passSubresources;

			std::vector<ResourceDescription> m_resourceDescriptions;
			std::vector<ResourceStateData *> m_externalResourceInfo;
			std::vector<std::vector<ResourceUsage>> m_resourceUsages; // m_resourceUsages[subresourceIndex][usageIndex]
			std::vector<uint32_t> m_resourceUsageOffsets;
			std::vector<std::pair<uint16_t, uint16_t>> m_resourceLifetimes;
			std::vector<bool> m_culledResources;

			std::vector<ResourceViewDescription> m_viewDescriptions;
			std::vector<std::vector<uint16_t>> m_viewUsages; // m_viewUsages[passIndex][viewIndex]
			std::vector<bool> m_culledViews;

			// allocated resources
			std::vector<ImageBuffer> m_imageBuffers;
			std::vector<ImageBufferView> m_imageBufferViews;
			CommandListFramePool m_commandListFramePool;
			gal::QueryPool *m_queryPools[3];

			// data used for recording
			std::vector<gal::Barrier> m_externalReleaseBarriers[3];
			std::vector<uint16_t> m_passHandleOrder;
			std::vector<gal::CommandList *> m_commandLists;
			std::vector<Batch> m_batches;
			std::vector<PassRecordInfo> m_passRecordInfo;

			uint64_t m_finalWaitValues[3];

			gal::Queue *getQueue(QueueType queueType) const;
			void forEachSubresource(ResourceViewHandle handle, std::function<void(uint32_t)> func);
			void createPasses(ResourceViewHandle finalResourceHandle, const ResourceStateData &finalResourceState);
			void createResources();
			void createSynchronization(ResourceViewHandle finalResourceHandle, bool forceWaitOnSemaphore, uint64_t waitValue);
			void record();
		};
	}
}