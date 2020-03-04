#pragma once
#include "Common.h"

namespace VEngine
{
	namespace gal
	{
		class Image;
		class Buffer;
		class ImageView;
		class BufferView;
		class QueryPool;
		class GraphicsPipeline;
		class ComputePipeline;
		class GraphicsDeviceQueue;
		class DescriptorSet;

		class CommandList
		{
		public:
			virtual ~CommandList() = 0;
			virtual void *getNativeHandle() const = 0;
			virtual void begin() = 0;
			virtual void end() = 0;
			virtual void bindPipeline(const GraphicsPipeline *pipeline) = 0;
			virtual void bindPipeline(const ComputePipeline *pipeline) = 0;
			virtual void setViewport(uint32_t firstViewport, uint32_t viewportCount, const Viewport *viewports) = 0;
			virtual void setScissor(uint32_t firstScissor, uint32_t scissorCount, const Rect *scissors) = 0;
			virtual void setLineWidth(float lineWidth) = 0;
			virtual void setDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) = 0;
			virtual void setBlendConstants(const float blendConstants[4]) = 0;
			virtual void setDepthBounds(float minDepthBounds, float maxDepthBounds) = 0;
			virtual void setStencilCompareMask(StencilFaceFlags faceMask, uint32_t compareMask) = 0;
			virtual void setStencilWriteMask(StencilFaceFlags faceMask, uint32_t writeMask) = 0;
			virtual void setStencilReference(StencilFaceFlags faceMask, uint32_t reference) = 0;
			virtual void bindDescriptorSets(const GraphicsPipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet **sets) = 0;
			virtual void bindDescriptorSets(const ComputePipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet **sets) = 0;
			virtual void bindIndexBuffer(const Buffer *buffer, uint64_t offset, IndexType indexType) = 0;
			virtual void bindVertexBuffers(uint32_t firstBinding, uint32_t count, const Buffer **buffers, uint64_t *offsets) = 0;
			virtual void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;
			virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) = 0;
			virtual void drawIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
			virtual void drawIndexedIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
			virtual void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
			virtual void dispatchIndirect(const Buffer *buffer, uint64_t offset) = 0;
			virtual void copyBuffer(const Buffer *srcBuffer, const Buffer *dstBuffer, uint32_t regionCount, const BufferCopy *regions) = 0;
			virtual void copyImage(const Image *srcImage, const Image *dstImage, uint32_t regionCount, const ImageCopy *regions) = 0;
			virtual void copyBufferToImage(const Buffer *srcBuffer, const Image *dstImage, uint32_t regionCount, const BufferImageCopy *regions) = 0;
			virtual void copyImageToBuffer(const Image *srcImage, const Buffer *dstBuffer, uint32_t regionCount, const BufferImageCopy *regions) = 0;
			virtual void updateBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t dataSize, const void *data) = 0;
			virtual void fillBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t size, uint32_t data) = 0;
			virtual void clearColorImage(const Image *image, const ClearColorValue *color, uint32_t rangeCount, const ImageSubresourceRange *ranges) = 0;
			virtual void clearDepthStencilImage(const Image *image, const ClearDepthStencilValue *depthStencil, uint32_t rangeCount, const ImageSubresourceRange *ranges) = 0;
			virtual void clearAttachments(uint32_t attachmentCount, const ClearAttachment *attachments, uint32_t rectCount, const ClearRect *rects) = 0;
			virtual void barrier(uint32_t count, const Barrier *barriers) = 0;
			virtual void beginQuery(const QueryPool *queryPool, uint32_t query) = 0;
			virtual void endQuery(const QueryPool *queryPool, uint32_t query) = 0;
			virtual void resetQueryPool(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount) = 0;
			virtual void writeTimestamp(PipelineStageFlagBits pipelineStage, const QueryPool *queryPool, uint32_t query) = 0;
			virtual void copyQueryPoolResults(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount, const Buffer *dstBuffer, uint64_t dstOffset, uint64_t stride, uint32_t flags) = 0;
			virtual void pushConstants(const GraphicsPipeline *pipeline, PipelineStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values) = 0;
			virtual void pushConstants(const ComputePipeline *pipeline, PipelineStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values) = 0;
			virtual void beginRenderPass(const RenderPassBeginInfo *renderPassBeginInfo) = 0;
			virtual void endRenderPass() = 0;
		};
	}
}