#pragma once
#include <cstdint>
#include <vector>
#include <map>
#include <functional>
#include "VKPipeline.h"
#include "Graphics/Vulkan/volk.h"

namespace VEngine
{
	typedef struct ImageViewHandle_t* ImageViewHandle;
	typedef struct ImageHandle_t* ImageHandle;
	typedef struct BufferViewHandle_t* BufferViewHandle;
	typedef struct BufferHandle_t* BufferHandle;
	typedef struct ResourceViewHandle_t* ResourceViewHandle;
	typedef struct ResourceHandle_t* ResourceHandle;

	class VKRenderGraph;
	class VKRenderGraph::PassResourceUsage;

	enum class QueueType
	{
		NONE = 0,
		GRAPHICS = 1 << 0,
		COMPUTE = 1 << 1,
		TRANSFER = 1 << 2,
	};

	union ClearValue
	{
		VkClearValue m_imageClearValue;
		uint32_t m_bufferClearValue;
	};

	struct ImageViewDescription
	{
		const char *m_name = "";
		bool m_clear = false;
		ClearValue m_clearValue;
		ImageHandle m_imageHandle;
		VkImageViewType m_viewType;
		VkFormat m_format;
		VkComponentMapping m_components;
		VkImageSubresourceRange m_subresourceRange;
	};

	struct ImageDescription
	{
		const char *m_name = "";
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_layers = 1;
		uint32_t m_levels = 1;
		uint32_t m_samples = 1;
		VkFormat m_format = VK_FORMAT_UNDEFINED;
	};

	struct BufferViewDescription
	{
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

	class VKRenderGraphPassBuilder
	{
	public:
		explicit VKRenderGraphPassBuilder(VKRenderGraph &graph, uint32_t passIndex);
		void readDepthStencil(ImageViewHandle handle, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
		void readInputAttachment(ImageViewHandle handle, uint32_t index, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
		void readTexture(ImageViewHandle handle, VkPipelineStageFlags stageFlags, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
		void readStorageImage(ImageViewHandle handle, VkPipelineStageFlags stageFlags, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
		void readStorageBuffer(BufferViewHandle handle, VkPipelineStageFlags stageFlags);
		void readUniformBuffer(BufferViewHandle handle, VkPipelineStageFlags stageFlags);
		void readVertexBuffer(BufferViewHandle handle);
		void readIndexBuffer(BufferViewHandle handle);
		void readIndirectBuffer(BufferViewHandle handle);
		void readImageTransfer(ImageViewHandle handle, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
		void readWriteStorageImage(ImageViewHandle handle, VkPipelineStageFlags stageFlags, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
		void readWriteStorageBuffer(BufferViewHandle handle, VkPipelineStageFlags stageFlags);
		void writeDepthStencil(ImageViewHandle handle, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
		void writeColorAttachment(ImageViewHandle handle, uint32_t index, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
		void writeStorageImage(ImageViewHandle handle, VkPipelineStageFlags stageFlags, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
		void writeStorageBuffer(BufferViewHandle handle, VkPipelineStageFlags stageFlags);
		void writeBufferTransfer(BufferViewHandle handle);
		void writeImageTransfer(ImageViewHandle handle, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
		ImageViewHandle createImageView(const ImageViewDescription &viewDesc);
		ImageHandle createImage(const ImageDescription &imageDesc);
		BufferViewHandle createBufferView(const BufferViewDescription &viewDesc);
		BufferHandle createBuffer(const BufferDescription &bufferDesc);

	private:
		VKRenderGraph &m_graph;
		uint32_t m_passIndex;

		void writeResourceUsages(ResourceViewHandle handle, uint32_t passIndex, VkPipelineStageFlags stageMask, uint32_t usageFlags, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM);
	};

	class VKRenderGraphRegistry
	{

	};

	class VKRenderGraph
	{
		friend class VKRenderGraphPassBuilder;
	public:
		typedef std::function<void(VkCommandBuffer cmdBuf, const VKRenderGraphRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)> RecordFunc;
		typedef std::function<RecordFunc(VKRenderGraphPassBuilder)> SetupFunc;

		explicit VKRenderGraph();
		VKRenderGraph(const VKRenderGraph &) = delete;
		VKRenderGraph(const VKRenderGraph &&) = delete;
		VKRenderGraph &operator= (const VKRenderGraph &) = delete;
		VKRenderGraph &operator= (const VKRenderGraph &&) = delete;
		~VKRenderGraph();
		void addGraphicsPass(const char *name, uint32_t width, uint32_t height, const SetupFunc &setupFunc);
		void addComputePass(const char *name, QueueType queueType, const SetupFunc &setupFunc);
		void addGenericPass(const char *name, QueueType queueType, const SetupFunc &setupFunc);

		void execute(ResourceHandle finalResourceHandle, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED);
		void reset();
		ImageViewHandle createImageView(const ImageViewDescription &viewDesc);
		ImageHandle createImage(const ImageDescription &imageDesc);
		BufferViewHandle createBufferView(const BufferViewDescription &viewDesc);
		BufferHandle createBuffer(const BufferDescription &bufferDesc);
		ImageHandle importImage(const ImageDescription &imageDescription, VkImage image, VkImageView imageView, VkImageLayout *layout, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore);
		BufferHandle importBuffer(const BufferDescription &bufferDescription, VkBuffer buffer, VkDeviceSize offset, VKAllocationHandle allocation, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore);


	private:

		struct PassResourceUsage
		{
			enum UsageFlags
			{
				READ_BIT = 1 << 0,
				WRITE_BIT = 1 << 1,
				TRANSFER_BIT = 1 << 2,
				STORAGE_BIT = 1 << 3,
				DEPTH_STENCIL_BIT = 1 << 4,
				ATTACHMENT_BIT = 1 << 5,
				TEXTURE_BIT = 1 << 6,
				UNIFORM_BIT = 1 << 7,
				INDEX_BIT = 1 << 8,
				VERTEX_BIT = 1 << 9,
				INDIRECT_BIT = 1 << 10,
			};
			uint32_t m_passIndex;
			VkPipelineStageFlags m_stageMask;
			uint32_t m_usageFlags;
			VkImageLayout m_finalLayout = VK_IMAGE_LAYOUT_MAX_ENUM;
		};

		struct PassDescription
		{
			enum PassType
			{
				GRAPHICS,
				COMPUTE,
				GENERIC,
				CLEAR
			};
			PassType m_type;
			QueueType m_queue;
			uint32_t m_framebufferInfoIndex;
		};

		struct ResourceDescription
		{
			const char *m_name = "";
			uint32_t m_width = 0;
			uint32_t m_height = 0;
			uint32_t m_layers = 1;
			uint32_t m_levels = 1;
			uint32_t m_samples = 1;
			VkFormat m_format = VK_FORMAT_UNDEFINED;
			VkDeviceSize m_size = 0;
			bool m_image;
		};

		struct ResourceViewDescription
		{
			const char *m_name = "";
			bool m_clear = false;
			ClearValue m_clearValue;
			ResourceHandle m_resourceHandle;
			VkImageViewType m_viewType;
			VkFormat m_format;
			VkComponentMapping m_components;
			VkImageSubresourceRange m_subresourceRange;
			VkDeviceSize m_size; // for buffers
			bool m_image;
		};

		struct FramebufferInfo
		{
			uint32_t m_width = 0;
			uint32_t m_height = 0;
			ImageViewHandle m_inputAttachments[VKRenderPassDescription::MAX_INPUT_ATTACHMENTS] = {};
			ImageViewHandle m_colorAttachments[VKRenderPassDescription::MAX_COLOR_ATTACHMENTS] = {};
			ImageViewHandle m_resolveAttachments[VKRenderPassDescription::MAX_RESOLVE_ATTACHMENTS] = {};
			ImageViewHandle m_depthStencilAttachment = 0;
		};

		std::vector<bool> m_culledPasses;
		std::vector<bool> m_culledResources;
		std::vector<bool> m_culledViews;
		std::vector<PassDescription> m_passDescriptions;
		std::vector<ResourceDescription> m_resourceDescriptions;
		std::vector<ResourceViewDescription> m_viewDescriptions;
		std::vector<FramebufferInfo> m_framebufferInfo;
		std::vector<uint32_t> m_resourceUsageOffsets;
		std::vector<std::vector<PassResourceUsage>> m_resourceUsages;

		void forEachSubresource(ResourceViewHandle handle, std::function<void(uint32_t)> func);
		void cull(ResourceViewHandle finalResourceViewHandle);
		void createResources();
	};
}