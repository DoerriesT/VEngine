#include "CommandListVk.h"
#include "PipelineVk.h"
#include "ResourceVk.h"
#include "QueueVk.h"
#include "Graphics/gal/QueryPool.h"
#include "Graphics/gal/Resource.h"
#include <algorithm>
#include <cassert>
#include "GraphicsDeviceVk.h"
#include "RenderPassDescriptionVk.h"
#include "UtilityVk.h"
#include "Graphics/gal/DescriptorSet.h"

VEngine::gal::CommandListVk::CommandListVk(VkCommandBuffer commandBuffer, GraphicsDeviceVk *device)
	:m_commandBuffer(commandBuffer),
	m_device(device)
{
}

void *VEngine::gal::CommandListVk::getNativeHandle() const
{
	return m_commandBuffer;
}

void VEngine::gal::CommandListVk::begin()
{
	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
}

void VEngine::gal::CommandListVk::end()
{
	vkEndCommandBuffer(m_commandBuffer);
}

void VEngine::gal::CommandListVk::bindPipeline(const GraphicsPipeline *pipeline)
{
	vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, (VkPipeline)pipeline->getNativeHandle());
}

void VEngine::gal::CommandListVk::bindPipeline(const ComputePipeline *pipeline)
{
	vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, (VkPipeline)pipeline->getNativeHandle());
}

void VEngine::gal::CommandListVk::setViewport(uint32_t firstViewport, uint32_t viewportCount, const Viewport *viewports)
{
	vkCmdSetViewport(m_commandBuffer, firstViewport, viewportCount, reinterpret_cast<const VkViewport *>(viewports));
}

void VEngine::gal::CommandListVk::setScissor(uint32_t firstScissor, uint32_t scissorCount, const Rect *scissors)
{
	vkCmdSetScissor(m_commandBuffer, firstScissor, scissorCount, reinterpret_cast<const VkRect2D *>(scissors));
}

void VEngine::gal::CommandListVk::setLineWidth(float lineWidth)
{
	vkCmdSetLineWidth(m_commandBuffer, lineWidth);
}

void VEngine::gal::CommandListVk::setDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
	vkCmdSetDepthBias(m_commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

void VEngine::gal::CommandListVk::setBlendConstants(const float blendConstants[4])
{
	vkCmdSetBlendConstants(m_commandBuffer, blendConstants);
}

void VEngine::gal::CommandListVk::setDepthBounds(float minDepthBounds, float maxDepthBounds)
{
	vkCmdSetDepthBounds(m_commandBuffer, minDepthBounds, maxDepthBounds);
}

void VEngine::gal::CommandListVk::setStencilCompareMask(StencilFaceFlags faceMask, uint32_t compareMask)
{
	vkCmdSetStencilCompareMask(m_commandBuffer, faceMask, compareMask);
}

void VEngine::gal::CommandListVk::setStencilWriteMask(StencilFaceFlags faceMask, uint32_t writeMask)
{
	vkCmdSetStencilWriteMask(m_commandBuffer, faceMask, writeMask);
}

void VEngine::gal::CommandListVk::setStencilReference(StencilFaceFlags faceMask, uint32_t reference)
{
	vkCmdSetStencilReference(m_commandBuffer, faceMask, reference);
}

void VEngine::gal::CommandListVk::bindDescriptorSets(const GraphicsPipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet **sets)
{
	const auto *pipelineVk = dynamic_cast<const GraphicsPipelineVk *>(pipeline);
	assert(pipelineVk);

	constexpr uint32_t batchSize = 8;
	const VkPipelineLayout pipelineLayoutVk = pipelineVk->getLayout();
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkDescriptorSet setsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			setsVk[j] = (VkDescriptorSet)sets[i * batchSize + j]->getNativeHandle();
		}
		vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayoutVk, firstSet + i * batchSize, countVk, setsVk, 0, nullptr);
	}
}

void VEngine::gal::CommandListVk::bindDescriptorSets(const ComputePipeline *pipeline, uint32_t firstSet, uint32_t count, const DescriptorSet **sets)
{
	const auto *pipelineVk = dynamic_cast<const ComputePipelineVk *>(pipeline);
	assert(pipelineVk);

	constexpr uint32_t batchSize = 8;
	const VkPipelineLayout pipelineLayoutVk = pipelineVk->getLayout();
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkDescriptorSet setsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			setsVk[j] = (VkDescriptorSet)sets[i * batchSize + j]->getNativeHandle();
		}
		vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutVk, firstSet + i * batchSize, countVk, setsVk, 0, nullptr);
	}
}

void VEngine::gal::CommandListVk::bindIndexBuffer(const Buffer *buffer, uint64_t offset, IndexType indexType)
{
	const auto *bufferVk = dynamic_cast<const BufferVk *>(buffer);
	assert(bufferVk);

	vkCmdBindIndexBuffer(m_commandBuffer, (VkBuffer)bufferVk->getNativeHandle(), bufferVk->getOffset(), static_cast<VkIndexType>(indexType));
}

void VEngine::gal::CommandListVk::bindVertexBuffers(uint32_t firstBinding, uint32_t count, const Buffer **buffers, uint64_t *offsets)
{
	constexpr uint32_t batchSize = 8;
	const uint32_t iterations = (count + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, count - i * batchSize);
		VkBuffer buffersVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			buffersVk[j] = (VkBuffer)buffers[i * batchSize + j]->getNativeHandle();
		}
		vkCmdBindVertexBuffers(m_commandBuffer, firstBinding + i * batchSize, countVk, buffersVk, offsets + uint64_t(i) * uint64_t(batchSize));
	}
}

void VEngine::gal::CommandListVk::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	vkCmdDraw(m_commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VEngine::gal::CommandListVk::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
	vkCmdDrawIndexed(m_commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VEngine::gal::CommandListVk::drawIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
{
	vkCmdDrawIndirect(m_commandBuffer, (VkBuffer)buffer->getNativeHandle(), offset, drawCount, stride);
}

void VEngine::gal::CommandListVk::drawIndexedIndirect(const Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
{
	vkCmdDrawIndexedIndirect(m_commandBuffer, (VkBuffer)buffer->getNativeHandle(), offset, drawCount, stride);
}

void VEngine::gal::CommandListVk::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	vkCmdDispatch(m_commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void VEngine::gal::CommandListVk::dispatchIndirect(const Buffer *buffer, uint64_t offset)
{
	vkCmdDispatchIndirect(m_commandBuffer, (VkBuffer)buffer->getNativeHandle(), offset);
}

void VEngine::gal::CommandListVk::copyBuffer(const Buffer *srcBuffer, const Buffer *dstBuffer, uint32_t regionCount, const BufferCopy *regions)
{
	vkCmdCopyBuffer(m_commandBuffer, (VkBuffer)srcBuffer->getNativeHandle(), (VkBuffer)dstBuffer->getNativeHandle(), regionCount, reinterpret_cast<const VkBufferCopy *>(regions));
}

void VEngine::gal::CommandListVk::copyImage(const Image *srcImage, const Image *dstImage, uint32_t regionCount, const ImageCopy *regions)
{
	constexpr uint32_t batchSize = 16;
	const VkImage srcImageVk = (VkImage)srcImage->getNativeHandle();
	const VkImage dstImageVk = (VkImage)dstImage->getNativeHandle();
	const VkImageAspectFlags srcAspectMask = UtilityVk::getImageAspectMask(static_cast<VkFormat>(srcImage->getDescription().m_format));
	const VkImageAspectFlags dstAspectMask = UtilityVk::getImageAspectMask(static_cast<VkFormat>(dstImage->getDescription().m_format));
	const uint32_t iterations = (regionCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, regionCount - i * batchSize);
		VkImageCopy regionsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &region = regions[i * batchSize + j];
			regionsVk[j] =
			{
				{ srcAspectMask, region.m_srcMipLevel, region.m_srcBaseLayer, region.m_srcLayerCount },
				*reinterpret_cast<const VkOffset3D *>(&region.m_srcOffset),
				{ dstAspectMask, region.m_dstMipLevel, region.m_dstBaseLayer, region.m_dstLayerCount },
				*reinterpret_cast<const VkOffset3D *>(&region.m_dstOffset),
				*reinterpret_cast<const VkExtent3D *>(&region.m_extent),
			};
		}
		vkCmdCopyImage(m_commandBuffer, srcImageVk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, countVk, regionsVk);
	}
}

void VEngine::gal::CommandListVk::copyBufferToImage(const Buffer *srcBuffer, const Image *dstImage, uint32_t regionCount, const BufferImageCopy *regions)
{
	constexpr uint32_t batchSize = 16;
	const VkBuffer srcBufferVk = (VkBuffer)srcBuffer->getNativeHandle();
	const VkImage dstImageVk = (VkImage)dstImage->getNativeHandle();

	const auto *bufferVk = dynamic_cast<const BufferVk *>(srcBuffer);
	assert(bufferVk);

	const uint64_t srcBufferOffset = bufferVk->getOffset();
	const VkImageAspectFlags dstAspectMask = UtilityVk::getImageAspectMask(static_cast<VkFormat>(dstImage->getDescription().m_format));
	const uint32_t iterations = (regionCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, regionCount - i * batchSize);
		VkBufferImageCopy regionsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &region = regions[i * batchSize + j];
			regionsVk[j] =
			{
				srcBufferOffset + region.m_bufferOffset,
				region.m_bufferRowLength,
				region.m_bufferImageHeight,
				{ dstAspectMask, region.m_imageMipLevel, region.m_imageBaseLayer, region.m_imageLayerCount },
				*reinterpret_cast<const VkOffset3D *>(&region.m_offset),
				*reinterpret_cast<const VkExtent3D *>(&region.m_extent),
			};
		}
		vkCmdCopyBufferToImage(m_commandBuffer, srcBufferVk, dstImageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, countVk, regionsVk);
	}
}

void VEngine::gal::CommandListVk::copyImageToBuffer(const Image *srcImage, const Buffer *dstBuffer, uint32_t regionCount, const BufferImageCopy *regions)
{
	constexpr uint32_t batchSize = 16;
	const VkImage srcImageVk = (VkImage)srcImage->getNativeHandle();
	const VkBuffer dstBufferVk = (VkBuffer)dstBuffer->getNativeHandle();
	

	const auto *bufferVk = dynamic_cast<const BufferVk *>(dstBuffer);
	assert(bufferVk);

	const uint64_t dstBufferOffset = bufferVk->getOffset();
	const VkImageAspectFlags dstAspectMask = UtilityVk::getImageAspectMask(static_cast<VkFormat>(srcImage->getDescription().m_format));
	const uint32_t iterations = (regionCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t countVk = std::min(batchSize, regionCount - i * batchSize);
		VkBufferImageCopy regionsVk[batchSize];
		for (uint32_t j = 0; j < countVk; ++j)
		{
			const auto &region = regions[i * batchSize + j];
			regionsVk[j] =
			{
				dstBufferOffset + region.m_bufferOffset,
				region.m_bufferRowLength,
				region.m_bufferImageHeight,
				{ dstAspectMask, region.m_imageMipLevel, region.m_imageBaseLayer, region.m_imageLayerCount },
				*reinterpret_cast<const VkOffset3D *>(&region.m_offset),
				*reinterpret_cast<const VkExtent3D *>(&region.m_extent),
			};
		}
		vkCmdCopyImageToBuffer(m_commandBuffer, srcImageVk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBufferVk, countVk, regionsVk);
	}
}

void VEngine::gal::CommandListVk::updateBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t dataSize, const void *data)
{
	vkCmdUpdateBuffer(m_commandBuffer, (VkBuffer)dstBuffer->getNativeHandle(), dstOffset, dataSize, data);
}

void VEngine::gal::CommandListVk::fillBuffer(const Buffer *dstBuffer, uint64_t dstOffset, uint64_t size, uint32_t data)
{
	vkCmdFillBuffer(m_commandBuffer, (VkBuffer)dstBuffer->getNativeHandle(), dstOffset, size, data);
}

void VEngine::gal::CommandListVk::clearColorImage(const Image *image, const ClearColorValue *color, uint32_t rangeCount, const ImageSubresourceRange *ranges)
{
	constexpr uint32_t batchSize = 8;
	const VkImage imageVk = (VkImage)image->getNativeHandle();
	const uint32_t iterations = (rangeCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t rangeCountVk = std::min(batchSize, rangeCount - i * batchSize);
		VkImageSubresourceRange rangesVk[batchSize];
		for (uint32_t j = 0; j < rangeCountVk; ++j)
		{
			const auto &range = ranges[i * batchSize + j];
			rangesVk[j] = { VK_IMAGE_ASPECT_COLOR_BIT, range.m_baseMipLevel, range.m_levelCount, range.m_baseArrayLayer, range.m_layerCount };
		}
		vkCmdClearColorImage(m_commandBuffer, imageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, reinterpret_cast<const VkClearColorValue *>(color), rangeCountVk, rangesVk);
	}
}

void VEngine::gal::CommandListVk::clearDepthStencilImage(const Image *image, const ClearDepthStencilValue *depthStencil, uint32_t rangeCount, const ImageSubresourceRange *ranges)
{
	constexpr uint32_t batchSize = 8;
	const VkImage imageVk = (VkImage)image->getNativeHandle();
	const VkImageAspectFlags imageAspectMask = UtilityVk::getImageAspectMask(static_cast<VkFormat>(image->getDescription().m_format));
	const uint32_t iterations = (rangeCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t rangeCountVk = std::min(batchSize, rangeCount - i * batchSize);
		VkImageSubresourceRange rangesVk[batchSize];
		for (uint32_t j = 0; j < rangeCountVk; ++j)
		{
			const auto &range = ranges[i * batchSize + j];
			rangesVk[j] = { imageAspectMask, range.m_baseMipLevel, range.m_levelCount, range.m_baseArrayLayer, range.m_layerCount };
		}
		vkCmdClearDepthStencilImage(m_commandBuffer, imageVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, reinterpret_cast<const VkClearDepthStencilValue *>(depthStencil), rangeCountVk, rangesVk);
	}
}

void VEngine::gal::CommandListVk::clearAttachments(uint32_t attachmentCount, const ClearAttachment *attachments, uint32_t rectCount, const ClearRect *rects)
{
	constexpr uint32_t batchSize = 8;
	const uint32_t iterations = (attachmentCount + (batchSize - 1)) / batchSize;
	for (uint32_t i = 0; i < iterations; ++i)
	{
		const uint32_t attachmentCountVk = std::min(batchSize, attachmentCount - i * batchSize);
		VkClearAttachment attachmentsVk[batchSize];
		for (uint32_t j = 0; j < attachmentCountVk; ++j)
		{
			const auto &attachment = attachments[i * batchSize + j];
			attachmentsVk[j] = 
			{ 
				static_cast<VkImageAspectFlags>(attachment.m_colorAttachment == 0xFFFFFFFF ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_DEPTH_BIT),  
				attachment.m_colorAttachment, 
				*reinterpret_cast<const VkClearValue*>(&attachment.m_clearValue) 
			};
		}
		vkCmdClearAttachments(m_commandBuffer, attachmentCount, attachmentsVk, rectCount, reinterpret_cast<const VkClearRect *>(rects));
	}
}

void VEngine::gal::CommandListVk::barrier(uint32_t count, const Barrier *barriers)
{
	constexpr uint32_t imageBarrierBatchSize = 16;
	constexpr uint32_t bufferBarrierBatchSize = 8;

	struct ResourceStateInfo
	{
		VkPipelineStageFlags m_stageMask;
		VkAccessFlags m_accessMask;
		VkImageLayout m_layout;
		bool m_readAccess;
		bool m_writeAccess;
	};

	auto getResourceStateInfo = [](ResourceState state, PipelineStageFlags stageFlags) -> ResourceStateInfo
	{
		switch (state)
		{
		case ResourceState::UNDEFINED:
			return { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED, false, false };
		case ResourceState::READ_DEPTH_STENCIL:
			return { VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, true, false };
		case ResourceState::READ_TEXTURE:
			return { stageFlags, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true, false };
		case ResourceState::READ_STORAGE_IMAGE:
			return { stageFlags, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, true, false };
		case ResourceState::READ_STORAGE_BUFFER:
			return { stageFlags, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		case ResourceState::READ_UNIFORM_BUFFER:
			return { stageFlags, VK_ACCESS_UNIFORM_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		case ResourceState::READ_VERTEX_BUFFER:
			return { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		case ResourceState::READ_INDEX_BUFFER:
			return { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		case ResourceState::READ_INDIRECT_BUFFER:
			return { VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		case ResourceState::READ_BUFFER_TRANSFER:
			return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, false };
		case ResourceState::READ_IMAGE_TRANSFER:
			return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, true, false };
		case ResourceState::READ_WRITE_STORAGE_IMAGE:
			return { stageFlags, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, true, true };
		case ResourceState::READ_WRITE_STORAGE_BUFFER:
			return { stageFlags, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, true, true };
		case ResourceState::READ_WRITE_DEPTH_STENCIL:
			return { VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, false, true };
		case ResourceState::WRITE_ATTACHMENT:
			return { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, false, true };
		case ResourceState::WRITE_STORAGE_IMAGE:
			return { stageFlags, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, false, true };
		case ResourceState::WRITE_STORAGE_BUFFER:
			return { stageFlags, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, false, true };
		case ResourceState::WRITE_BUFFER_TRANSFER:
			return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, false, true };
		case ResourceState::WRITE_IMAGE_TRANSFER:
			return { VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false, true };
		case ResourceState::PRESENT_IMAGE:
			return { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, true, false };
		default:
			assert(false);
			break;
		}
		return {};
	};

	uint32_t i = 0;

	while (i < count)
	{
		uint32_t imageBarrierCount = 0;
		uint32_t bufferBarrierCount = 0;

		VkImageMemoryBarrier imageBarriers[imageBarrierBatchSize];
		VkBufferMemoryBarrier bufferBarriers[bufferBarrierBatchSize];
		VkMemoryBarrier memoryBarrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

		for (; i < count && bufferBarrierCount < bufferBarrierBatchSize && imageBarrierCount < imageBarrierBatchSize; ++i)
		{
			const auto &barrier = barriers[i];
			assert(bool(barrier.m_image) != bool(barrier.m_buffer));

			const auto beforeStateInfo = getResourceStateInfo(barrier.m_stateBefore, barrier.m_stagesBefore);
			const auto afterStateInfo = getResourceStateInfo(barrier.m_stateAfter, barrier.m_stagesAfter);

			const bool imageBarrierRequired = barrier.m_image && (beforeStateInfo.m_layout != afterStateInfo.m_layout || barrier.m_queueOwnershipAcquireBarrier || barrier.m_queueOwnershipReleaseBarrier);
			const bool bufferBarrierRequired = barrier.m_buffer && (barrier.m_queueOwnershipAcquireBarrier || barrier.m_queueOwnershipReleaseBarrier);
			const bool memoryBarrierRequired = beforeStateInfo.m_writeAccess && !imageBarrierRequired && !bufferBarrierRequired;
			const bool executionBarrierRequired = beforeStateInfo.m_writeAccess || afterStateInfo.m_writeAccess || memoryBarrierRequired || bufferBarrierRequired || imageBarrierRequired;

			if (imageBarrierRequired)
			{
				const auto &subResRange = barrier.m_imageSubresourceRange;
				const VkImageAspectFlags imageAspectMask = UtilityVk::getImageAspectMask(static_cast<VkFormat>(barrier.m_image->getDescription().m_format));

				auto &imageBarrier = imageBarriers[imageBarrierCount++];
				imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				imageBarrier.srcAccessMask = beforeStateInfo.m_accessMask;
				imageBarrier.dstAccessMask = afterStateInfo.m_accessMask;
				imageBarrier.oldLayout = beforeStateInfo.m_layout;
				imageBarrier.newLayout = afterStateInfo.m_layout;
				imageBarrier.srcQueueFamilyIndex = barrier.m_srcQueue ? barrier.m_srcQueue->getFamilyIndex() : VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.dstQueueFamilyIndex = barrier.m_dstQueue ? barrier.m_dstQueue->getFamilyIndex() : VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.image = (VkImage)barrier.m_image->getNativeHandle();
				imageBarrier.subresourceRange = { imageAspectMask, subResRange.m_baseMipLevel, subResRange.m_levelCount, subResRange.m_baseArrayLayer, subResRange.m_layerCount };
			}
			else if (bufferBarrierRequired)
			{
				auto &bufferBarrier = bufferBarriers[bufferBarrierCount++];
				bufferBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
				bufferBarrier.srcAccessMask = beforeStateInfo.m_accessMask;
				bufferBarrier.dstAccessMask = afterStateInfo.m_accessMask;
				bufferBarrier.srcQueueFamilyIndex = barrier.m_srcQueue ? barrier.m_srcQueue->getFamilyIndex() : VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.dstQueueFamilyIndex = barrier.m_dstQueue ? barrier.m_dstQueue->getFamilyIndex() : VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.buffer = (VkBuffer)barrier.m_buffer->getNativeHandle();
				bufferBarrier.offset = 0;
				bufferBarrier.size = VK_WHOLE_SIZE;
			}

			if (memoryBarrierRequired)
			{
				memoryBarrier.srcAccessMask = beforeStateInfo.m_accessMask;
				memoryBarrier.dstAccessMask = afterStateInfo.m_accessMask;
			}

			if (executionBarrierRequired)
			{
				srcStages |= beforeStateInfo.m_stageMask;
				dstStages |= afterStateInfo.m_stageMask;
			}
		}

		if (bufferBarrierCount || imageBarrierCount || memoryBarrier.srcAccessMask || srcStages != VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT || dstStages != VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)
		{
			vkCmdPipelineBarrier(m_commandBuffer, srcStages, dstStages, 0, 1, &memoryBarrier, bufferBarrierCount, bufferBarriers, imageBarrierCount, imageBarriers);
		}
	}
}

void VEngine::gal::CommandListVk::beginQuery(const QueryPool *queryPool, uint32_t query)
{
	vkCmdBeginQuery(m_commandBuffer, (VkQueryPool)queryPool->getNativeHandle(), query, 0);
}

void VEngine::gal::CommandListVk::endQuery(const QueryPool *queryPool, uint32_t query)
{
	vkCmdEndQuery(m_commandBuffer, (VkQueryPool)queryPool->getNativeHandle(), query);
}

void VEngine::gal::CommandListVk::resetQueryPool(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount)
{
	vkCmdResetQueryPool(m_commandBuffer, (VkQueryPool)queryPool->getNativeHandle(), firstQuery, queryCount);
}

void VEngine::gal::CommandListVk::writeTimestamp(PipelineStageFlagBits pipelineStage, const QueryPool *queryPool, uint32_t query)
{
	vkCmdWriteTimestamp(m_commandBuffer, static_cast<VkPipelineStageFlagBits>(pipelineStage), (VkQueryPool)queryPool->getNativeHandle(), query);
}

void VEngine::gal::CommandListVk::copyQueryPoolResults(const QueryPool *queryPool, uint32_t firstQuery, uint32_t queryCount, const Buffer *dstBuffer, uint64_t dstOffset, uint64_t stride, uint32_t flags)
{
	vkCmdCopyQueryPoolResults(m_commandBuffer, (VkQueryPool)queryPool->getNativeHandle(), firstQuery, queryCount, (VkBuffer)dstBuffer->getNativeHandle(), dstOffset, stride, flags);
}

void VEngine::gal::CommandListVk::pushConstants(const GraphicsPipeline *pipeline, PipelineStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values)
{
	const auto *pipelineVk = dynamic_cast<const GraphicsPipelineVk *>(pipeline);
	assert(pipelineVk);
	vkCmdPushConstants(m_commandBuffer, pipelineVk->getLayout(), stageFlags, offset, size, values);
}

void VEngine::gal::CommandListVk::pushConstants(const ComputePipeline *pipeline, PipelineStageFlags stageFlags, uint32_t offset, uint32_t size, const void *values)
{
	const auto *pipelineVk = dynamic_cast<const ComputePipelineVk *>(pipeline);
	assert(pipelineVk);
	vkCmdPushConstants(m_commandBuffer, pipelineVk->getLayout(), stageFlags, offset, size, values);
}

void VEngine::gal::CommandListVk::beginRenderPass(uint32_t colorAttachmentCount, ColorAttachmentDescription *colorAttachments, DepthStencilAttachmentDescription *depthStencilAttachment, Rect renderArea)
{
	assert(colorAttachmentCount <= 8);

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkFramebuffer framebuffer = VK_NULL_HANDLE;

	VkClearValue clearValues[9] = {};
	VkImageView attachmentViews[9] = {};
	RenderPassDescriptionVk::ColorAttachmentDescriptionVk colorAttachmentDescsVk[8] = {};
	RenderPassDescriptionVk::DepthStencilAttachmentDescriptionVk depthStencilAttachmentDescVk = {};
	
	uint32_t attachmentCount = 0;
	
	auto translateLoadOp = [](AttachmentLoadOp loadOp)
	{
		switch (loadOp)
		{
		case AttachmentLoadOp::LOAD:
			return VK_ATTACHMENT_LOAD_OP_LOAD;
		case AttachmentLoadOp::CLEAR:
			return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case AttachmentLoadOp::DONT_CARE:
			return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		default:
			return VK_ATTACHMENT_LOAD_OP_LOAD;
		}
	};

	auto translateStoreOp = [](AttachmentStoreOp storeOp)
	{
		switch (storeOp)
		{
		case AttachmentStoreOp::STORE:
			return VK_ATTACHMENT_STORE_OP_STORE;
		case AttachmentStoreOp::DONT_CARE:
			return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		default:
			return VK_ATTACHMENT_STORE_OP_STORE;
		}
	};

	// fill out color attachment info structs
	for (uint32_t i = 0; i < colorAttachmentCount; ++i)
	{
		const auto &attachment = colorAttachments[i];
		const auto *image = attachment.m_imageView->getImage();

		attachmentViews[i] = (VkImageView)attachment.m_imageView->getNativeHandle();
		clearValues[i].color = *reinterpret_cast<const VkClearColorValue *>(&attachment.m_clearValue);

		auto &imageDesc = image->getDescription();

		auto &attachmentDesc = colorAttachmentDescsVk[i];
		attachmentDesc = {};
		attachmentDesc.m_format = static_cast<VkFormat>(imageDesc.m_format);
		attachmentDesc.m_samples = static_cast<VkSampleCountFlagBits>(imageDesc.m_samples);
		attachmentDesc.m_loadOp = translateLoadOp(attachment.m_loadOp);
		attachmentDesc.m_storeOp = translateStoreOp(attachment.m_storeOp);

		++attachmentCount;
	}

	// fill out depth/stencil attachment info struct
	if (depthStencilAttachment)
	{
		const auto &attachment = *depthStencilAttachment;
		const auto *image = attachment.m_imageView->getImage();

		attachmentViews[attachmentCount] = (VkImageView)attachment.m_imageView->getNativeHandle();
		clearValues[attachmentCount].depthStencil = *reinterpret_cast<const VkClearDepthStencilValue *>(&attachment.m_clearValue);
		
		auto &imageDesc = image->getDescription();

		auto &attachmentDesc = depthStencilAttachmentDescVk;
		attachmentDesc = {};
		attachmentDesc.m_format = static_cast<VkFormat>(imageDesc.m_format);
		attachmentDesc.m_samples = static_cast<VkSampleCountFlagBits>(imageDesc.m_samples);
		attachmentDesc.m_loadOp = translateLoadOp(attachment.m_loadOp);
		attachmentDesc.m_storeOp = translateStoreOp(attachment.m_storeOp);
		attachmentDesc.m_stencilLoadOp = translateLoadOp(attachment.m_stencilLoadOp);
		attachmentDesc.m_stencilStoreOp = translateStoreOp(attachment.m_stencilStoreOp);
		attachmentDesc.m_layout = attachment.m_readOnly ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		++attachmentCount;
	}

	// get renderpass
	{
		RenderPassDescriptionVk renderPassDescription;
		renderPassDescription.setColorAttachments(colorAttachmentCount, colorAttachmentDescsVk);
		if (depthStencilAttachment)
		{
			renderPassDescription.setDepthStencilAttachment(depthStencilAttachmentDescVk);
		}
		renderPassDescription.finalize();

		renderPass = m_device->getRenderPass(renderPassDescription);
	}

	// create framebuffer
	{
		VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = attachmentCount;
		framebufferCreateInfo.pAttachments = attachmentViews;
		framebufferCreateInfo.width = renderArea.m_extent.m_width;
		framebufferCreateInfo.height = renderArea.m_extent.m_height;
		framebufferCreateInfo.layers = 1;

		UtilityVk::checkResult(vkCreateFramebuffer(m_device->getDevice(), &framebufferCreateInfo, nullptr, &framebuffer), "Failed to create Framebuffer!");
		m_device->addToDeletionQueue(framebuffer);
	}

	VkRenderPassBeginInfo renderPassBeginInfoVk{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderPassBeginInfoVk.renderPass = renderPass;
	renderPassBeginInfoVk.framebuffer = framebuffer;
	renderPassBeginInfoVk.renderArea = *reinterpret_cast<const VkRect2D *>(&renderArea);
	renderPassBeginInfoVk.clearValueCount = attachmentCount;
	renderPassBeginInfoVk.pClearValues = clearValues;

	vkCmdBeginRenderPass(m_commandBuffer, &renderPassBeginInfoVk, VK_SUBPASS_CONTENTS_INLINE);
}

void VEngine::gal::CommandListVk::endRenderPass()
{
	vkCmdEndRenderPass(m_commandBuffer);
}
