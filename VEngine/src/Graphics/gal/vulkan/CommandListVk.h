#pragma once
#include "Graphics/gal/GraphicsAbstractionLayer.h"
#include "volk.h"

namespace VEngine
{
	namespace gal
	{
		class GraphicsDeviceVk;

		class CommandListVk : public CommandList
		{
		public:
			explicit CommandListVk(VkCommandBuffer commandBuffer, GraphicsDeviceVk *device);
			void *getNativeHandle() const;
			void begin() override;
			void end() override;
			void bindPipeline(const GraphicsPipeline *pipeline) override;
			void bindPipeline(const ComputePipeline *pipeline) override;
			void setViewport(uint32_t firstViewport, uint32_t viewportCount, const Viewport *viewports) override;
			void setScissor(uint32_t firstScissor, uint32_t scissorCount, const Rect *scissors) override;
			void setLineWidth(float lineWidth) override;
			void setDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) override;
			void setBlendConstants(const float blendConstants[4]) override;
			void setDepthBounds(float minDepthBounds, float maxDepthBounds) override;
			void setStencilCompareMask(StencilFaceFlags faceMask, uint32_t compareMask) override;
			void setStencilWriteMask(StencilFaceFlags faceMask, uint32_t writeMask) override;
			void setStencilReference(StencilFaceFlags faceMask, uint32_t reference) override;
			void bindDescriptorSets(const GraphicsPipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet *const *sets) override;
			void bindDescriptorSets(const ComputePipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet *const *sets) override;
			void bindIndexBuffer(const Buffer *buffer, uint64_t offset, IndexType indexType) override;
			void bindVertexBuffers(uint32_t firstBinding, uint32_t count, const Buffer *const *buffers, uint64_t *offsets) override;
			void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
			void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override;
			void drawIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;
			void drawIndexedIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;
			void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
			void dispatchIndirect(const Buffer *buffer, uint64_t offset) override;
			void copyBuffer(const Buffer *srcBuffer, const Buffer *dstBuffer, uint32_t regionCount, const BufferCopy *regions) override;
			void copyImage(const Image *srcImage, const Image *dstImage, uint32_t regionCount, const ImageCopy *regions) override;
			void copyBufferToImage(const Buffer *srcBuffer, const Image *dstImage, uint32_t regionCount, const BufferImageCopy *regions) override;
			void copyImageToBuffer(const Image *srcImage, const Buffer *dstBuffer, uint32_t regionCount, const BufferImageCopy *regions) override;
			void updateBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t dataSize, const void *data) override;
			void fillBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t size, uint32_t data) override;
			void clearColorImage(const Image *image, const ClearColorValue *color, uint32_t rangeCount, const ImageSubresourceRange *ranges) override;
			void clearDepthStencilImage(const Image *image, const ClearDepthStencilValue *depthStencil, uint32_t rangeCount, const ImageSubresourceRange *ranges) override;
			void barrier(uint32_t count, const Barrier *barriers) override;
			void beginQuery(const QueryPool *queryPool, uint32_t query) override;
			void endQuery(const QueryPool *queryPool, uint32_t query) override;
			void resetQueryPool(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount) override;
			void writeTimestamp(PipelineStageFlags pipelineStage, const QueryPool *queryPool, uint32_t query) override;
			void copyQueryPoolResults(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount, const Buffer *dstBuffer, uint64_t dstOffset) override;
			void pushConstants(const GraphicsPipeline *pipeline, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values) override;
			void pushConstants(const ComputePipeline *pipeline, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values) override;
			void beginRenderPass(uint32_t colorAttachmentCount, ColorAttachmentDescription *colorAttachments, DepthStencilAttachmentDescription *depthStencilAttachment, const Rect &renderArea, bool rwTextureBufferAccess) override;
			void endRenderPass() override;
			void insertDebugLabel(const char *label) override;
			virtual void beginDebugLabel(const char *label) override;
			virtual void endDebugLabel() override;

		private:
			VkCommandBuffer m_commandBuffer;
			GraphicsDeviceVk *m_device;
		};
	}
}