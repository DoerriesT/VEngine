#include "FrameGraph.h"
#include <cassert>
#include <stack>
#include "Utility/Utility.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKUtility.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include "Utility/ContainerUtility.h"
#include "Graphics/Vulkan/VKSyncPrimitiveAllocator.h"

using namespace VEngine::FrameGraph;


VEngine::FrameGraph::PassBuilder::PassBuilder(Graph &graph, size_t passIndex)
	:m_graph(graph),
	m_passIndex(passIndex)
{
}

void PassBuilder::setDimensions(uint32_t width, uint32_t height)
{
	m_graph.m_framebufferInfo[m_passIndex].m_width = width;
	m_graph.m_framebufferInfo[m_passIndex].m_height = height;
}

void PassBuilder::readDepthStencil(ImageHandle imageHandle)
{
	size_t resourceIndex = (size_t)imageHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;
	m_graph.m_attachmentResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	resourceUsage.m_accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	resourceUsage.m_usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	resourceUsage.m_imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.m_framebufferInfo[m_passIndex].m_depthStencilAttachment = imageHandle;
}

void PassBuilder::readInputAttachment(ImageHandle imageHandle, uint32_t index, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement)
{
	size_t resourceIndex = (size_t)imageHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;
	m_graph.m_attachmentResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	resourceUsage.m_accessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	resourceUsage.m_usageFlags = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	resourceUsage.m_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.addDescriptorWrite(m_passIndex, resourceIndex, set, binding, arrayElement, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, resourceUsage.m_imageLayout, VK_NULL_HANDLE, 0);
}

void PassBuilder::readTexture(ImageHandle imageHandle, VkPipelineStageFlags stageFlags, VkSampler sampler, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement)
{
	size_t resourceIndex = (size_t)imageHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = stageFlags;
	resourceUsage.m_accessMask = VK_ACCESS_SHADER_READ_BIT;
	resourceUsage.m_usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
	resourceUsage.m_imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.addDescriptorWrite(m_passIndex, resourceIndex, set, binding, arrayElement, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, resourceUsage.m_imageLayout, sampler, 0);
}

void PassBuilder::readStorageImage(ImageHandle imageHandle, VkPipelineStageFlags stageFlags, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement)
{
	size_t resourceIndex = (size_t)imageHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = stageFlags;
	resourceUsage.m_accessMask = VK_ACCESS_SHADER_READ_BIT;
	resourceUsage.m_usageFlags = VK_IMAGE_USAGE_STORAGE_BIT;
	resourceUsage.m_imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.addDescriptorWrite(m_passIndex, resourceIndex, set, binding, arrayElement, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, resourceUsage.m_imageLayout, VK_NULL_HANDLE, 0);
}

void PassBuilder::readStorageBuffer(BufferHandle bufferHandle, VkPipelineStageFlags stageFlags, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement)
{
	size_t resourceIndex = (size_t)bufferHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = stageFlags;
	resourceUsage.m_accessMask = VK_ACCESS_SHADER_READ_BIT;
	resourceUsage.m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.addDescriptorWrite(m_passIndex, resourceIndex, set, binding, arrayElement, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_IMAGE_LAYOUT_UNDEFINED, VK_NULL_HANDLE, 0);
}

void PassBuilder::readStorageBufferDynamic(BufferHandle bufferHandle, VkPipelineStageFlags stageFlags, VkDeviceSize dynamicBufferSize, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement)
{
	size_t resourceIndex = (size_t)bufferHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = stageFlags;
	resourceUsage.m_accessMask = VK_ACCESS_SHADER_READ_BIT;
	resourceUsage.m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.addDescriptorWrite(m_passIndex, resourceIndex, set, binding, arrayElement, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_IMAGE_LAYOUT_UNDEFINED, VK_NULL_HANDLE, dynamicBufferSize);
}

void PassBuilder::readUniformBuffer(BufferHandle bufferHandle, VkPipelineStageFlags stageFlags, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement)
{
	size_t resourceIndex = (size_t)bufferHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = stageFlags;
	resourceUsage.m_accessMask = VK_ACCESS_UNIFORM_READ_BIT;
	resourceUsage.m_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.addDescriptorWrite(m_passIndex, resourceIndex, set, binding, arrayElement, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_IMAGE_LAYOUT_UNDEFINED, VK_NULL_HANDLE, 0);
}

void PassBuilder::readUniformBufferDynamic(BufferHandle bufferHandle, VkPipelineStageFlags stageFlags, VkDeviceSize dynamicBufferSize, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement)
{
	size_t resourceIndex = (size_t)bufferHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = stageFlags;
	resourceUsage.m_accessMask = VK_ACCESS_UNIFORM_READ_BIT;
	resourceUsage.m_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.addDescriptorWrite(m_passIndex, resourceIndex, set, binding, arrayElement, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_IMAGE_LAYOUT_UNDEFINED, VK_NULL_HANDLE, dynamicBufferSize);
}

void PassBuilder::readVertexBuffer(BufferHandle bufferHandle)
{
	size_t resourceIndex = (size_t)bufferHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	resourceUsage.m_accessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	resourceUsage.m_usageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
}

void PassBuilder::readIndexBuffer(BufferHandle bufferHandle)
{
	size_t resourceIndex = (size_t)bufferHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	resourceUsage.m_accessMask = VK_ACCESS_INDEX_READ_BIT;
	resourceUsage.m_usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
}

void PassBuilder::readIndirectBuffer(BufferHandle bufferHandle)
{
	size_t resourceIndex = (size_t)bufferHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
	resourceUsage.m_accessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	resourceUsage.m_usageFlags = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
}

void PassBuilder::readImageTransfer(ImageHandle imageHandle)
{
	size_t resourceIndex = (size_t)imageHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_readResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	resourceUsage.m_accessMask = VK_ACCESS_TRANSFER_READ_BIT;
	resourceUsage.m_usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	resourceUsage.m_imageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
}

void PassBuilder::writeDepthStencil(ImageHandle imageHandle)
{
	size_t resourceIndex = (size_t)imageHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_writeResources[resourceIndex][m_passIndex] = true;
	m_graph.m_attachmentResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	resourceUsage.m_accessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	resourceUsage.m_usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	resourceUsage.m_imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.m_framebufferInfo[m_passIndex].m_depthStencilAttachment = imageHandle;
}

void PassBuilder::writeColorAttachment(ImageHandle imageHandle, uint32_t index)
{
	size_t resourceIndex = (size_t)imageHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_writeResources[resourceIndex][m_passIndex] = true;
	m_graph.m_attachmentResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	resourceUsage.m_accessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	resourceUsage.m_usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	resourceUsage.m_imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.m_framebufferInfo[m_passIndex].m_colorAttachments[index] = imageHandle;
}

void PassBuilder::writeStorageImage(ImageHandle imageHandle, VkPipelineStageFlags stageFlags, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement)
{
	size_t resourceIndex = (size_t)imageHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_writeResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = stageFlags;
	resourceUsage.m_accessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	resourceUsage.m_usageFlags = VK_IMAGE_USAGE_STORAGE_BIT;
	resourceUsage.m_imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.addDescriptorWrite(m_passIndex, resourceIndex, set, binding, arrayElement, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, resourceUsage.m_imageLayout, VK_NULL_HANDLE, 0);
}

void PassBuilder::writeStorageBuffer(BufferHandle bufferHandle, VkPipelineStageFlags stageFlags, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement)
{
	size_t resourceIndex = (size_t)bufferHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_writeResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = stageFlags;
	resourceUsage.m_accessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	resourceUsage.m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.addDescriptorWrite(m_passIndex, resourceIndex, set, binding, arrayElement, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_IMAGE_LAYOUT_UNDEFINED, VK_NULL_HANDLE, 0);
}

void PassBuilder::writeStorageBufferDynamic(BufferHandle bufferHandle, VkPipelineStageFlags stageFlags, VkDeviceSize dynamicBufferSize, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement)
{
	size_t resourceIndex = (size_t)bufferHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_writeResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = stageFlags;
	resourceUsage.m_accessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	resourceUsage.m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
	m_graph.addDescriptorWrite(m_passIndex, resourceIndex, set, binding, arrayElement, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_IMAGE_LAYOUT_UNDEFINED, VK_NULL_HANDLE, dynamicBufferSize);
}

void VEngine::FrameGraph::PassBuilder::writeImageTransfer(ImageHandle imageHandle)
{
	size_t resourceIndex = (size_t)imageHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_writeResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	resourceUsage.m_accessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	resourceUsage.m_usageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	resourceUsage.m_imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
}

void PassBuilder::writeBufferFromHost(BufferHandle bufferHandle)
{
	size_t resourceIndex = (size_t)bufferHandle - 1;

	m_graph.m_accessedResources[resourceIndex][m_passIndex] = true;
	m_graph.m_writeResources[resourceIndex][m_passIndex] = true;

	auto &resourceUsage = m_graph.m_resourceUsages[resourceIndex][m_passIndex];
	resourceUsage.m_stageMask = VK_PIPELINE_STAGE_HOST_BIT;
	resourceUsage.m_accessMask = VK_ACCESS_HOST_WRITE_BIT;
	resourceUsage.m_usageFlags = 0;

	m_graph.m_passStageMasks[m_passIndex] |= resourceUsage.m_stageMask;
}

ImageHandle PassBuilder::createImage(const ImageDescription &imageDesc)
{
	return m_graph.createImage(imageDesc);
}

BufferHandle PassBuilder::createBuffer(const BufferDescription &bufferDesc)
{
	return m_graph.createBuffer(bufferDesc);
}

Graph::Graph(VEngine::VKSyncPrimitiveAllocator &syncPrimitiveAllocator)
	:m_syncPrimitiveAllocator(syncPrimitiveAllocator)
{
	VkCommandPoolCreateInfo poolCreateInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	poolCreateInfo.queueFamilyIndex = g_context.m_queueFamilyIndices.m_graphicsFamily;
	if (vkCreateCommandPool(g_context.m_device, &poolCreateInfo, nullptr, &m_graphicsCommandPool) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create Command Pool!", -1);
	}

	poolCreateInfo.queueFamilyIndex = g_context.m_queueFamilyIndices.m_computeFamily;
	if (vkCreateCommandPool(g_context.m_device, &poolCreateInfo, nullptr, &m_computeCommandPool) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create Command Pool!", -1);
	}

	poolCreateInfo.queueFamilyIndex = g_context.m_queueFamilyIndices.m_transferFamily;
	if (vkCreateCommandPool(g_context.m_device, &poolCreateInfo, nullptr, &m_transferCommandPool) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create Command Pool!", -1);
	}

	VkCommandBufferAllocateInfo cmdBufAllocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocInfo.commandBufferCount = MAX_PASSES;

	cmdBufAllocInfo.commandPool = m_graphicsCommandPool;
	if (vkAllocateCommandBuffers(g_context.m_device, &cmdBufAllocInfo, m_commandBuffers[0]) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to allocate command buffers!", -1);
	}

	cmdBufAllocInfo.commandPool = m_computeCommandPool;
	if (vkAllocateCommandBuffers(g_context.m_device, &cmdBufAllocInfo, m_commandBuffers[1]) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to allocate command buffers!", -1);
	}

	cmdBufAllocInfo.commandPool = m_transferCommandPool;
	if (vkAllocateCommandBuffers(g_context.m_device, &cmdBufAllocInfo, m_commandBuffers[2]) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to allocate command buffers!", -1);
	}
}

Graph::~Graph()
{
	if (m_fence)
	{
		reset();
	}

	vkDestroyCommandPool(g_context.m_device, m_graphicsCommandPool, nullptr);
	vkDestroyCommandPool(g_context.m_device, m_computeCommandPool, nullptr);
	vkDestroyCommandPool(g_context.m_device, m_transferCommandPool, nullptr);
}

PassBuilder VEngine::FrameGraph::Graph::addPass(const char *name, PassType passType, QueueType queueType, Pass *pass)
{
	size_t passIndex = 0;

	// graphics and compute passes may need a clear pass before, blit and host access dont.
	// clear passes are inserted implicitly and must not be added by the user
	switch (passType)
	{
	case PassType::GRAPHICS:
	case PassType::COMPUTE:
		assert(m_passCount + 1 < MAX_PASSES);
		m_passCount += 2;
		passIndex = m_passCount - 1;

		// add potential clear pass
		m_passNames[passIndex - 1] = "Clear";
		m_passTypes[passIndex - 1] = PassType::CLEAR;
		m_queueType[passIndex - 1] = queueType;
		break;

	case PassType::BLIT:
	case PassType::HOST_ACCESS:
		assert(m_passCount < MAX_PASSES);
		passIndex = m_passCount++;
		break;

	case PassType::CLEAR:
		assert(false);
		break;
	default:
		assert(false);
		break;
	}

	// add actual pass
	m_passNames[passIndex] = name;
	m_passTypes[passIndex] = passType;
	m_queueType[passIndex] = queueType;

	m_passes[passIndex] = pass;

	return PassBuilder(*this, passIndex);
}

void Graph::execute(ResourceHandle finalResourceHandle)
{
	std::bitset<MAX_DESCRIPTOR_SETS> culledDescriptorSets;
	size_t firstResourceUses[MAX_RESOURCES];
	size_t lastResourceUses[MAX_RESOURCES];
	SyncBits syncBits[MAX_PASSES] = {};

	cull(culledDescriptorSets, firstResourceUses, lastResourceUses, finalResourceHandle);
	createClearPasses(firstResourceUses);
	createResources();
	createRenderPasses(firstResourceUses, lastResourceUses, finalResourceHandle);
	createSynchronization(firstResourceUses, lastResourceUses, syncBits);
	writeDescriptorSets(culledDescriptorSets);
	recordAndSubmit(firstResourceUses, lastResourceUses, syncBits, finalResourceHandle);
}

void Graph::reset()
{
	// wait for completion of work
	if (m_fence != VK_NULL_HANDLE)
	{
		vkWaitForFences(g_context.m_device, 1, &m_fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	}

	// release semaphores
	for (size_t i = 0; i < MAX_PASSES; ++i)
	{
		if (m_semaphores[i] != VK_NULL_HANDLE)
		{
			m_syncPrimitiveAllocator.freeSemaphore(m_semaphores[i]);
		}
	}

	// release events
	for (size_t i = 0; i < MAX_PASSES * 2; ++i)
	{
		if (m_events[i] != VK_NULL_HANDLE)
		{
			m_syncPrimitiveAllocator.freeEvent(m_events[i]);
		}
	}

	// destroy internal resources
	for (size_t i = 0; i < m_resourceCount; ++i)
	{
		if (m_culledResources[i] && !m_externalResources[i])
		{
			if (m_imageResources[i])
			{
				g_context.m_allocator.destroyImage(m_images[i], m_allocations[i]);
				vkDestroyImageView(g_context.m_device, m_imageViews[i], nullptr);
			}
			else
			{
				g_context.m_allocator.destroyBuffer(m_buffers[i], m_allocations[i]);
			}
			assert(m_allocationCount);
			--m_allocationCount;
		}
	}

	assert(m_allocationCount == 0);

	// destroy renderpasses and framebuffers
	for (size_t i = 0; i < m_passCount; ++i)
	{
		if (m_culledPasses[i] && m_passTypes[i] == PassType::GRAPHICS)
		{
			vkDestroyRenderPass(g_context.m_device, m_renderpassFramebufferHandles[i].first, nullptr);
			vkDestroyFramebuffer(g_context.m_device, m_renderpassFramebufferHandles[i].second, nullptr);
		}
	}

	// destroy fence
	if (m_fence)
	{
		m_syncPrimitiveAllocator.freeFence(m_fence);
	}

	// reset commandpool
	vkResetCommandPool(g_context.m_device, m_graphicsCommandPool, 0);
	vkResetCommandPool(g_context.m_device, m_computeCommandPool, 0);
	vkResetCommandPool(g_context.m_device, m_transferCommandPool, 0);

	// zero out data
	m_graphicsCommandBufferCount = 0;
	m_computeCommandBufferCount = 0;
	m_transferCommandBufferCount = 0;
	m_resourceCount = 0;
	m_passCount = 0;
	m_descriptorSetCount = 0;
	m_descriptorWriteCount = 0;
	memset(&m_writeResources, 0, sizeof(m_writeResources));
	memset(&m_readResources, 0, sizeof(m_readResources));
	memset(&m_accessedResources, 0, sizeof(m_accessedResources));
	memset(&m_attachmentResources, 0, sizeof(m_attachmentResources));
	memset(&m_accessedDescriptorSets, 0, sizeof(m_accessedDescriptorSets));
	m_culledPasses = 0;
	m_culledResources = 0;
	m_concurrentResources = 0;
	m_externalResources = 0;
	m_imageResources = 0;
	m_clearResources = 0;
	memset(m_framebufferInfo, 0, sizeof(m_framebufferInfo));
	memset(m_resourceUsages, 0, sizeof(m_resourceUsages));
	m_fence = 0;
	memset(m_semaphores, 0, sizeof(m_semaphores));
	memset(m_events, 0, sizeof(m_events));
	memset(m_passStageMasks, 0, sizeof(m_passStageMasks));
	memset(m_descriptorSets, 0, sizeof(m_descriptorSets));
	memset(m_descriptorWrites, 0, sizeof(m_descriptorWrites));
}

ImageHandle Graph::createImage(const ImageDescription &imageDesc)
{
	size_t resourceIndex = m_resourceCount++;

	m_imageResources[resourceIndex] = true;
	m_resourceNames[resourceIndex] = imageDesc.m_name;
	m_concurrentResources[resourceIndex] = imageDesc.m_concurrent;
	m_clearResources[resourceIndex] = imageDesc.m_clear;
	m_clearValues[resourceIndex] = imageDesc.m_clearValue;
	m_resourceDescriptions[resourceIndex].m_width = imageDesc.m_width;
	m_resourceDescriptions[resourceIndex].m_height = imageDesc.m_height;
	m_resourceDescriptions[resourceIndex].m_layers = imageDesc.m_layers;
	m_resourceDescriptions[resourceIndex].m_levels = imageDesc.m_levels;
	m_resourceDescriptions[resourceIndex].m_samples = imageDesc.m_samples;
	m_resourceDescriptions[resourceIndex].m_format = imageDesc.m_format;

	return ImageHandle(resourceIndex + 1);
}

BufferHandle Graph::createBuffer(const BufferDescription &bufferDesc)
{
	size_t resourceIndex = m_resourceCount++;

	m_resourceNames[resourceIndex] = bufferDesc.m_name;
	m_concurrentResources[resourceIndex] = bufferDesc.m_concurrent;
	m_clearResources[resourceIndex] = bufferDesc.m_clear;
	m_clearValues[resourceIndex] = bufferDesc.m_clearValue;
	m_resourceDescriptions[resourceIndex].m_size = bufferDesc.m_size;
	m_resourceDescriptions[resourceIndex].m_hostVisible = bufferDesc.m_hostVisible;

	return BufferHandle(resourceIndex + 1);
}

ImageHandle Graph::importImage(const ImageDescription &imageDescription, VkImage image, VkImageView imageView, VkImageLayout *layout, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore)
{
	size_t resourceIndex = m_resourceCount++;

	m_externalResources[resourceIndex] = true;
	m_imageResources[resourceIndex] = true;
	m_resourceNames[resourceIndex] = imageDescription.m_name;
	m_concurrentResources[resourceIndex] = imageDescription.m_concurrent;
	m_clearResources[resourceIndex] = imageDescription.m_clear;
	m_clearValues[resourceIndex] = imageDescription.m_clearValue;
	m_resourceDescriptions[resourceIndex].m_width = imageDescription.m_width;
	m_resourceDescriptions[resourceIndex].m_height = imageDescription.m_height;
	m_resourceDescriptions[resourceIndex].m_layers = imageDescription.m_layers;
	m_resourceDescriptions[resourceIndex].m_levels = imageDescription.m_levels;
	m_resourceDescriptions[resourceIndex].m_samples = imageDescription.m_samples;
	m_resourceDescriptions[resourceIndex].m_format = imageDescription.m_format;

	m_images[resourceIndex] = image;
	m_imageViews[resourceIndex] = imageView;
	m_externalLayouts[resourceIndex] = layout;

	m_semaphores[MAX_PASSES + resourceIndex * 2] = waitSemaphore;
	m_semaphores[MAX_PASSES + resourceIndex * 2 + 1] = signalSemaphore;

	return ImageHandle(resourceIndex + 1);
}

BufferHandle Graph::importBuffer(const BufferDescription &bufferDescription, VkBuffer buffer, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore)
{
	size_t resourceIndex = m_resourceCount++;

	m_externalResources[resourceIndex] = true;
	m_resourceNames[resourceIndex] = bufferDescription.m_name;
	m_concurrentResources[resourceIndex] = bufferDescription.m_concurrent;
	m_clearResources[resourceIndex] = bufferDescription.m_clear;
	m_clearValues[resourceIndex] = bufferDescription.m_clearValue;
	m_resourceDescriptions[resourceIndex].m_size = bufferDescription.m_size;
	m_resourceDescriptions[resourceIndex].m_hostVisible = bufferDescription.m_hostVisible;

	m_buffers[resourceIndex] = buffer;

	m_semaphores[MAX_PASSES + resourceIndex * 2] = waitSemaphore;
	m_semaphores[MAX_PASSES + resourceIndex * 2 + 1] = signalSemaphore;

	return BufferHandle(resourceIndex + 1);
}

void Graph::cull(std::bitset<MAX_DESCRIPTOR_SETS> &culledDescriptorSets, size_t *firstResourceUses, size_t *lastResourceUses, ResourceHandle finalResourceHandle)
{
	// indexed by pass index
	uint32_t passRefCounts[MAX_PASSES] = {};

	// indexed by ResourceHandle - 1
	uint32_t resourceRefCounts[MAX_RESOURCES] = {};

	// compute initial ref counts
	for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
	{
		for (size_t passIndex = 0; passIndex < m_passCount; ++passIndex)
		{
			// increment pass ref count for each written resource
			if (m_writeResources[resourceIndex][passIndex])
			{
				++passRefCounts[passIndex];
			}
			// increment resource ref count for each reading pass
			if (m_readResources[resourceIndex][passIndex])
			{
				++resourceRefCounts[resourceIndex];
			}
		}
	}

	// increment ref count on final resource handle
	++resourceRefCounts[(size_t)finalResourceHandle - 1];

	std::stack<size_t> resourceStack;

	// fill stack with resources with refCount == 0
	for (size_t i = 0; i < m_resourceCount; ++i)
	{
		if (resourceRefCounts[i] == 0)
		{
			resourceStack.push(i);
		}
	}

	// cull passes/resources
	while (!resourceStack.empty())
	{
		size_t resourceIndex = resourceStack.top();
		resourceStack.pop();

		// find writing passes
		for (size_t passIndex = 0; passIndex < m_passCount; ++passIndex)
		{
			// if writing pass, decrement refCount. if it falls to zero, decrement refcount of read resources
			if (m_writeResources[resourceIndex][passIndex]
				&& passRefCounts[passIndex] != 0
				&& --passRefCounts[passIndex] == 0)
			{
				for (size_t resIndex = 0; resIndex < m_resourceCount; ++resIndex)
				{
					// if read resource, decrement resource refCount. if it falls to zero, add it to stack
					if (m_readResources[resIndex][passIndex]
						&& resourceRefCounts[resIndex] != 0
						&& --resourceRefCounts[resIndex] == 0)
					{
						resourceStack.push(resIndex);
					}
				}
			}
		}
	}

	// initialize resource lifetime arrays
	for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
	{
		firstResourceUses[resourceIndex] = ~size_t(0);
		lastResourceUses[resourceIndex] = 0;
	}

	// set bits for remaining passes and all resources accessed by them
	for (size_t passIndex = 0; passIndex < m_passCount; ++passIndex)
	{
		if (passRefCounts[passIndex])
		{
			m_culledPasses[passIndex] = true;

			for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
			{
				if (m_accessedResources[resourceIndex][passIndex])
				{
					m_culledResources[resourceIndex] = true;

					// update resource lifetimes
					firstResourceUses[resourceIndex] = std::min(passIndex, firstResourceUses[resourceIndex]);
					lastResourceUses[resourceIndex] = std::max(passIndex, lastResourceUses[resourceIndex]);
				}
			}
		}
		else
		{
			// remove all access bits on all resources accessed in the culled pass
			for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
			{
				m_accessedResources[resourceIndex][passIndex] = false;
				m_writeResources[resourceIndex][passIndex] = false;
				m_readResources[resourceIndex][passIndex] = false;
			}
		}
	}

	// cull descriptor sets
	for (size_t descriptorSetIndex = 0; descriptorSetIndex < m_descriptorSetCount; ++descriptorSetIndex)
	{
		for (size_t passIndex = 0; passIndex < m_passCount; ++passIndex)
		{
			// skip culled passes and remove their bit
			if (!m_culledPasses[passIndex])
			{
				m_accessedDescriptorSets[descriptorSetIndex][passIndex] = false;
				continue;
			}

			// if we found a pass using this descriptor set, we can stop searching
			if (m_accessedDescriptorSets[descriptorSetIndex][passIndex])
			{
				culledDescriptorSets[descriptorSetIndex] = true;
				break;
			}
		}
	}
}

void Graph::createClearPasses(size_t *firstResourceUses)
{
	for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
	{
		size_t firstUseIndex = firstResourceUses[resourceIndex];

		// skip if resource is culled or does not need to be cleared
		if (!m_culledResources[resourceIndex]
			|| !m_clearResources[resourceIndex]
			|| (m_passTypes[firstUseIndex] == PassType::GRAPHICS
				&& m_attachmentResources[resourceIndex][firstUseIndex]))
		{
			continue;
		}

		size_t clearPassIndex = firstUseIndex - 1;

		// we only inserted clear passes for graphics and compute passes.
		// clearing resources first used for blit or host writes is useless
		assert(m_passTypes[clearPassIndex] == PassType::CLEAR);

		// update first use
		firstResourceUses[resourceIndex] = clearPassIndex;

		// update culled passes
		m_culledPasses[clearPassIndex] = true;

		// set access bits
		m_writeResources[resourceIndex][clearPassIndex] = true;
		m_accessedResources[resourceIndex][clearPassIndex] = true;

		// update stage mask and resource usage
		m_passStageMasks[clearPassIndex] |= VK_PIPELINE_STAGE_TRANSFER_BIT;
		m_resourceUsages[resourceIndex][clearPassIndex].m_stageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		m_resourceUsages[resourceIndex][clearPassIndex].m_accessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		m_resourceUsages[resourceIndex][clearPassIndex].m_usageFlags = m_imageResources[resourceIndex] ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		m_resourceUsages[resourceIndex][clearPassIndex].m_imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
}

void Graph::createResources()
{
	for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
	{
		// skip culled or external resources
		if (!m_culledResources[resourceIndex] || m_externalResources[resourceIndex])
		{
			continue;
		}

		// find usage flags and used queues
		VkFlags usageFlags = 0;
		uint32_t queueFamilyIndices[3] = {};
		size_t queueFamilyCount = 0;
		for (size_t passIndex = 0; passIndex < m_passCount; ++passIndex)
		{
			if (m_culledPasses[passIndex])
			{
				usageFlags |= m_resourceUsages[resourceIndex][passIndex].m_usageFlags;

				if (m_queueType[passIndex] != QueueType::NONE)
				{
					uint32_t queueFamilyIndex = queueIndexFromQueueType(m_queueType[passIndex]);

					// is index already in array?
					bool containsIndex = false;
					for (size_t i = 0; i < queueFamilyCount; ++i)
					{
						if (queueFamilyIndices[i] == queueFamilyIndex)
						{
							containsIndex = true;
							break;
						}
					}

					if (!containsIndex)
					{
						queueFamilyIndices[queueFamilyCount++] = queueFamilyIndex;
					}
				}
			}
		}

		// concurrent access is not necessary if only one queue uses the resource
		if (queueFamilyCount <= 1)
		{
			m_concurrentResources[resourceIndex] = false;
		}

		// is resource image or buffer?
		if (m_imageResources[resourceIndex])
		{
			// create image
			{
				const auto &desc = m_resourceDescriptions[resourceIndex];

				VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
				imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imageCreateInfo.format = desc.m_format;
				imageCreateInfo.extent.width = desc.m_width;
				imageCreateInfo.extent.height = desc.m_height;
				imageCreateInfo.extent.depth = 1;
				imageCreateInfo.mipLevels = desc.m_levels;
				imageCreateInfo.arrayLayers = 1;
				imageCreateInfo.samples = VkSampleCountFlagBits(desc.m_samples);
				imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.usage = usageFlags;
				imageCreateInfo.sharingMode = m_concurrentResources[resourceIndex] ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
				imageCreateInfo.queueFamilyIndexCount = queueFamilyCount;
				imageCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
				imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

				VKAllocationCreateInfo allocCreateInfo = {};
				allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
				
				if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_images[resourceIndex], m_allocations[resourceIndex]) != VK_SUCCESS)
				{
					Utility::fatalExit("Failed to create image!", -1);
				}

				++m_allocationCount;
			}

			// create image view
			{
				VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				viewInfo.image = m_images[resourceIndex];
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewInfo.format = m_resourceDescriptions[resourceIndex].m_format;
				viewInfo.subresourceRange =
				{
					VKUtility::imageAspectMaskFromFormat(viewInfo.format),
					0,
					VK_REMAINING_MIP_LEVELS ,
					0,
					1
				};

				if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &m_imageViews[resourceIndex]) != VK_SUCCESS)
				{
					Utility::fatalExit("Failed to create image view!", -1);
				}
			}
		}
		else
		{
			// create buffer
			{
				const auto &desc = m_resourceDescriptions[resourceIndex];

				VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
				bufferCreateInfo.size = desc.m_size;
				bufferCreateInfo.usage = usageFlags;
				bufferCreateInfo.sharingMode = m_concurrentResources[resourceIndex] ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
				bufferCreateInfo.queueFamilyIndexCount = queueFamilyCount;
				bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

				VKAllocationCreateInfo allocCreateInfo = {};
				allocCreateInfo.m_requiredFlags = desc.m_hostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
				allocCreateInfo.m_preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

				if (g_context.m_allocator.createBuffer(allocCreateInfo, bufferCreateInfo, m_buffers[resourceIndex], m_allocations[resourceIndex]) != VK_SUCCESS)
				{
					Utility::fatalExit("Failed to create buffer!", -1);
				}

				++m_allocationCount;
			}
		}
	}
}

void Graph::createRenderPasses(size_t *firstResourceUses, size_t *lastResourceUses, ResourceHandle finalResourceHandle)
{
	for (size_t passIndex = 0; passIndex < m_passCount; ++passIndex)
	{
		// skip culled passes and non graphics passes
		if (!m_culledPasses[passIndex] || m_passTypes[passIndex] != PassType::GRAPHICS)
		{
			continue;
		}

		// required data
		VkAttachmentDescription attachmentDescriptions[MAX_COLOR_ATTACHMENTS + 1];
		VkAttachmentReference attachmentRefs[MAX_COLOR_ATTACHMENTS + 1];
		VkSubpassDependency dependencies[2] =
		{
			{ VK_SUBPASS_EXTERNAL , 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_passStageMasks[passIndex] },
			{ 0, VK_SUBPASS_EXTERNAL, m_passStageMasks[passIndex], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT }
		};
		VkSubpassDescription subpass = {};

		const auto &framebufferInfo = m_framebufferInfo[passIndex];

#ifdef _DEBUG
		// make sure there are no empty handles in between non-empty handles on the color attachment array
		{
			bool foundEmpty = false;
			for (size_t i = 0; i < MAX_COLOR_ATTACHMENTS; ++i)
			{
				if (!framebufferInfo.m_colorAttachments[i])
				{
					foundEmpty = true;
					continue;
				}

				// if we end up here, we have a non-empty handle, so assert all previous handles were non-empty aswell
				assert(!foundEmpty);
			}
		}
#endif //_DEBUG

		size_t attachmentCount = 0;

		for (size_t i = 0; i < MAX_COLOR_ATTACHMENTS + 1; ++i)
		{
			auto handle = i < MAX_COLOR_ATTACHMENTS ? framebufferInfo.m_colorAttachments[i] : framebufferInfo.m_depthStencilAttachment;

			// skip empty handles
			if (!handle)
			{
				continue;
			}

			++attachmentCount;

			size_t resourceIndex = (size_t)handle - 1;

			// find previous and next pass accessing this resource
			size_t previousPassIndex = ContainerUtility::findPreviousSetBit(m_accessedResources[resourceIndex], passIndex);
			size_t nextPassIndex = ContainerUtility::findNextSetBit(m_accessedResources[resourceIndex], passIndex);

			// update dependencies
			{
				const auto &previousUsage = m_resourceUsages[resourceIndex][previousPassIndex];
				const auto &currentUsage = m_resourceUsages[resourceIndex][passIndex];
				const auto &nextUsage = m_resourceUsages[resourceIndex][nextPassIndex];

				// if layout does not stay the same or access is not read -> read, dependency is needed
				if (previousPassIndex != passIndex
					&& (previousUsage.m_imageLayout != currentUsage.m_imageLayout
						|| m_writeResources[resourceIndex][previousPassIndex]
						|| m_writeResources[resourceIndex][passIndex]))
				{
					dependencies[0].srcStageMask |= previousUsage.m_stageMask;
					dependencies[0].dstStageMask |= currentUsage.m_stageMask;
					dependencies[0].srcAccessMask |= previousUsage.m_accessMask;
					dependencies[0].dstAccessMask |= currentUsage.m_accessMask;
				}

				// if layout does not stay the same or access is not read -> read, dependency is needed
				if (nextPassIndex != passIndex &&
					(currentUsage.m_imageLayout != nextUsage.m_imageLayout
						|| m_writeResources[resourceIndex][passIndex]
						|| m_writeResources[resourceIndex][nextPassIndex]))
				{
					dependencies[1].srcStageMask |= currentUsage.m_stageMask;
					dependencies[1].dstStageMask |= nextUsage.m_stageMask;
					dependencies[1].srcAccessMask |= currentUsage.m_accessMask;
					dependencies[1].dstAccessMask |= nextUsage.m_accessMask;
				}
			}

			// create attachment ref and description
			{
				attachmentRefs[attachmentCount - 1] = { static_cast<uint32_t>(attachmentCount - 1),  m_resourceUsages[resourceIndex][passIndex].m_imageLayout };

				const auto &desc = m_resourceDescriptions[resourceIndex];

				// create attachment description
				VkAttachmentDescription &descr = attachmentDescriptions[attachmentCount - 1];
				descr.flags = 0;
				descr.format = desc.m_format;
				descr.samples = VkSampleCountFlagBits(desc.m_samples);
				descr.loadOp = (passIndex == firstResourceUses[resourceIndex] && m_clearResources[resourceIndex]) ? VK_ATTACHMENT_LOAD_OP_CLEAR	// first use and clear? -> clear
					: (passIndex == firstResourceUses[resourceIndex] && !m_clearResources[resourceIndex]) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE		// first use and not clear? -> dont care
					: (passIndex == lastResourceUses[resourceIndex]) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE												// last use? -> dont care
					: VK_ATTACHMENT_LOAD_OP_LOAD;																									// neither first nor last use -> load
				descr.storeOp = VK_ATTACHMENT_STORE_OP_STORE;// (passIndex == lastResourceUses[resourceIndex] || resourceIndex == ((size_t)finalResourceHandle - 1)) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
				descr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				descr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				descr.initialLayout = m_externalResources[resourceIndex] ? *m_externalLayouts[resourceIndex] // external resources have special initial layout
					: previousPassIndex != passIndex ? m_resourceUsages[resourceIndex][previousPassIndex].m_imageLayout // internal resources use previous layout
					: VK_IMAGE_LAYOUT_UNDEFINED; // internal resource on first use is undefined layout
				descr.finalLayout = attachmentRefs[attachmentCount - 1].layout;
			}
		}

		// create subpass
		{
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = static_cast<uint32_t>(attachmentCount - (framebufferInfo.m_depthStencilAttachment ? 1 : 0));
			subpass.pColorAttachments = attachmentRefs;
			subpass.pDepthStencilAttachment = (framebufferInfo.m_depthStencilAttachment ? &attachmentRefs[attachmentCount - 1] : nullptr);
		}

		// create renderpass
		{
			VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentCount);
			renderPassInfo.pAttachments = attachmentDescriptions;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies;

			if (vkCreateRenderPass(g_context.m_device, &renderPassInfo, nullptr, &m_renderpassFramebufferHandles[passIndex].first) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create render pass!", -1);
			}
		}


		// create framebuffer
		{
			VkImageView framebufferAttachments[MAX_COLOR_ATTACHMENTS + 1];

			size_t attachmentIndex = 0;

			// color attachments
			for (size_t i = 0; i < MAX_COLOR_ATTACHMENTS; ++i)
			{
				auto handle = framebufferInfo.m_colorAttachments[i];

				if (!handle)
				{
					break;
				}

				framebufferAttachments[attachmentIndex++] = m_imageViews[(size_t)handle - 1];
			}

			// depth attachment
			if (framebufferInfo.m_depthStencilAttachment)
			{
				framebufferAttachments[attachmentCount - 1] = m_imageViews[(size_t)framebufferInfo.m_depthStencilAttachment - 1];
			}

			VkFramebufferCreateInfo framebufferCreateInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferCreateInfo.renderPass = m_renderpassFramebufferHandles[passIndex].first;
			framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachmentCount);
			framebufferCreateInfo.pAttachments = framebufferAttachments;
			framebufferCreateInfo.width = framebufferInfo.m_width;
			framebufferCreateInfo.height = framebufferInfo.m_height;
			framebufferCreateInfo.layers = 1;

			if (vkCreateFramebuffer(g_context.m_device, &framebufferCreateInfo, nullptr, &m_renderpassFramebufferHandles[passIndex].second) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create framebuffer!", -1);
			}
		}
	}
}

void Graph::createSynchronization(size_t *firstResourceUses, size_t *lastResourceUses, SyncBits *syncBits)
{
	// holds the index of the first dependent pass
	size_t semaphoreDependencies[MAX_PASSES];
	memset(semaphoreDependencies, 0xFFFFFFFF, sizeof(semaphoreDependencies));
	// holds the number of passes waiting on this one
	size_t eventDependencies[MAX_PASSES] = {};

	// find event/semaphore dependencies and image layout transitions
	for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
	{
		// skip culled resources
		if (!m_culledResources[resourceIndex])
		{
			continue;
		}

		size_t firstUseIndex = firstResourceUses[resourceIndex];
		size_t lastUseIndex = lastResourceUses[resourceIndex];
		bool imageResource = m_imageResources[resourceIndex];
		bool readOnly = true;
		for (size_t passIndex = firstUseIndex; passIndex <= lastUseIndex; ++passIndex)
		{
			if (m_writeResources[resourceIndex][passIndex] && m_passTypes[passIndex] != PassType::HOST_ACCESS)
			{
				readOnly = false;
				break;
			}
		}

		size_t previousPassIndex = firstUseIndex;
		VkImageLayout previousLayout = imageResource && m_externalResources[resourceIndex] ? *m_externalLayouts[resourceIndex] : VK_IMAGE_LAYOUT_UNDEFINED;
		bool previousWrite = false;
		QueueType previousQueueType = QueueType::NONE;

		for (size_t passIndex = firstUseIndex; passIndex <= lastUseIndex; ++passIndex)
		{
			// skip if pass is culled or does not access resource or is host write
			if (!m_culledPasses[passIndex] || !m_accessedResources[resourceIndex][passIndex] || m_passTypes[passIndex] == PassType::HOST_ACCESS)
			{
				continue;
			}

			// layout transitions of renderpass attachments are handled automatically, so dont include them here
			bool imageLayoutTransition = imageResource && previousLayout != m_resourceUsages[resourceIndex][passIndex].m_imageLayout && !m_attachmentResources[resourceIndex][passIndex];
			bool executionDependency = previousPassIndex != passIndex && !previousWrite && m_writeResources[resourceIndex][passIndex];
			bool memoryDependency = previousPassIndex != passIndex && previousWrite;
			bool queueChange = previousQueueType != QueueType::NONE && previousQueueType != m_queueType[passIndex];
			bool exclusiveResource = !m_concurrentResources[resourceIndex];

			bool scheduleSemaphore = false;

			if (queueChange)
			{
				// schedule semaphore if ownership needs to be transfered, layout changes or we might have WAR/RAW hazards
				if (exclusiveResource || imageLayoutTransition || !readOnly)
				{
					semaphoreDependencies[previousPassIndex] = std::min(semaphoreDependencies[previousPassIndex], passIndex);
					scheduleSemaphore = true;
				}

				// release resource in previous pass if we need to do an ownership transfer
				if (exclusiveResource)
				{
					assert(syncBits[previousPassIndex].m_releaseCount < MAX_RESOURCE_BARRIERS);
					++syncBits[previousPassIndex].m_releaseCount;
					syncBits[previousPassIndex].m_releaseResources[resourceIndex] = true;
				}
			}

			// schedule resource barrier in current pass to either acquire the resource (ownership transfer) or to change the layout
			if ((queueChange && exclusiveResource) || imageLayoutTransition)
			{
				assert(syncBits[passIndex].m_resourceBarrierCount < MAX_RESOURCE_BARRIERS);
				++syncBits[passIndex].m_resourceBarrierCount;
				syncBits[passIndex].m_barrierResources[resourceIndex] = true;
			}

			if (!scheduleSemaphore)
			{
				// we need to wait on an event for both execution and memory dependencies
				if (executionDependency || memoryDependency)
				{
					assert(syncBits[passIndex].m_waitEventCount < MAX_WAIT_EVENTS);
					// update count of events to wait on
					syncBits[passIndex].m_waitEventCount += syncBits[passIndex].m_waitEvents[previousPassIndex] ? 0 : 1;
					// update count of passes waiting on previousPassIndex
					eventDependencies[previousPassIndex] += syncBits[passIndex].m_waitEvents[previousPassIndex] ? 0 : 1;
					syncBits[passIndex].m_waitEvents[previousPassIndex] = true;
				}
			}

			previousPassIndex = passIndex;
			previousLayout = m_resourceUsages[resourceIndex][passIndex].m_imageLayout;
			previousWrite = m_writeResources[resourceIndex][passIndex];
			previousQueueType = m_queueType[passIndex];
		}
	}

	// remove redundant wait events and create semaphores/events
	for (size_t passIndex = 0; passIndex < m_passCount; ++passIndex)
	{
		// skip culled passes
		if (!m_culledPasses[passIndex])
		{
			continue;
		}

		// find last pass waiting on a semaphore on this queue
		size_t waitingPassIndex = 0;
		bool foundWaitingPass = false;
		for (size_t i = 0; i <= passIndex; ++i)
		{
			if (m_queueType[i] == m_queueType[passIndex] && syncBits[i].m_waitSemaphoreCount)
			{
				waitingPassIndex = i;
				foundWaitingPass = true;
			}
		}

		// we can discard all waits on events from before the waiting pass, as the semaphore wait handles synchronization
		if (foundWaitingPass)
		{
			for (size_t i = 0; i < waitingPassIndex; ++i)
			{
				if (syncBits[passIndex].m_waitEvents[i])
				{
					assert(syncBits[passIndex].m_waitEventCount);
					assert(eventDependencies[i]);
					syncBits[passIndex].m_waitEvents[i] = false;
					--syncBits[passIndex].m_waitEventCount;
					--eventDependencies[i];
				}
			}
		}

		// create semaphore
		if (semaphoreDependencies[passIndex] != ~size_t(0))
		{
			assert(m_semaphores[passIndex] == VK_NULL_HANDLE);
			m_semaphores[passIndex] = m_syncPrimitiveAllocator.acquireSemaphore();

			size_t dependentPassIndex = semaphoreDependencies[passIndex];

			syncBits[dependentPassIndex].m_waitSemaphoreCount += syncBits[dependentPassIndex].m_waitSemaphores[passIndex] ? 0 : 1;
			assert(syncBits[dependentPassIndex].m_waitSemaphoreCount < MAX_WAIT_SEMAPHORES);
			syncBits[dependentPassIndex].m_waitSemaphores[passIndex] = true;
		}

		// create event
		if (eventDependencies[passIndex])
		{
			assert(m_events[passIndex] == VK_NULL_HANDLE);
			m_events[passIndex] = m_syncPrimitiveAllocator.acquireEvent();
		}

		// update wait event memory dependency access masks and source stage masks
		for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
		{
			// skip if resource is not accessed by this pass
			if (!m_accessedResources[resourceIndex][passIndex])
			{
				continue;
			}

			for (size_t waitPassIndex = 0; waitPassIndex < passIndex; ++waitPassIndex)
			{
				// if we need to wait on this pass, update masks
				if (syncBits[passIndex].m_waitEvents[waitPassIndex])
				{
					syncBits[passIndex].m_waitEventsSrcStageMask |= m_passStageMasks[waitPassIndex];

					// memory dependency
					if (m_writeResources[resourceIndex][waitPassIndex])
					{
						syncBits[passIndex].m_memoryBarrierSrcAccessMask |= m_resourceUsages[resourceIndex][waitPassIndex].m_accessMask;
						syncBits[passIndex].m_memoryBarrierDstAccessMask |= m_resourceUsages[resourceIndex][passIndex].m_accessMask;
					}
				}
			}
		}
	}

	// insert resource semaphores
	for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
	{
		if (m_culledResources[resourceIndex] && m_externalResources[resourceIndex])
		{
			if (m_semaphores[MAX_PASSES + resourceIndex * 2])
			{
				assert(m_passTypes[firstResourceUses[resourceIndex]] != PassType::HOST_ACCESS);
				size_t passIndex = firstResourceUses[resourceIndex];
				++syncBits[passIndex].m_waitSemaphoreCount;
				assert(syncBits[passIndex].m_waitSemaphoreCount < MAX_WAIT_SEMAPHORES);
				syncBits[passIndex].m_waitSemaphores[MAX_PASSES + resourceIndex * 2] = true;
			}
			if (m_semaphores[MAX_PASSES + resourceIndex * 2 + 1])
			{
				assert(m_passTypes[lastResourceUses[resourceIndex]] != PassType::HOST_ACCESS);
				size_t passIndex = lastResourceUses[resourceIndex];
				++syncBits[passIndex].m_signalSemaphoreCount;
				assert(syncBits[passIndex].m_signalSemaphoreCount < MAX_SIGNAL_SEMAPHORES);
				syncBits[passIndex].m_signalSemaphores[MAX_PASSES + resourceIndex * 2 + 1] = true;
			}
		}
	}
}

void Graph::writeDescriptorSets(std::bitset<MAX_DESCRIPTOR_SETS> &culledDescriptorSets)
{
	union
	{
		VkDescriptorImageInfo m_imageInfo;
		VkDescriptorBufferInfo m_bufferInfo;
	} infos[MAX_DESCRIPTOR_WRITES];

	VkWriteDescriptorSet writes[MAX_DESCRIPTOR_WRITES];
	size_t writeCount = 0;

	for (size_t i = 0; i < m_descriptorWriteCount; ++i)
	{
		const auto &write = m_descriptorWrites[i];

		// skip culled sets and resources
		if (!culledDescriptorSets[write.m_descriptorSetIndex] || !m_culledResources[write.m_resourceIndex])
		{
			continue;
		}

		VkWriteDescriptorSet &descriptorWrite = writes[writeCount];
		descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		descriptorWrite.dstSet = m_descriptorSets[write.m_descriptorSetIndex];
		descriptorWrite.dstBinding = write.m_binding;
		descriptorWrite.dstArrayElement = write.m_arrayIndex;
		descriptorWrite.descriptorType = write.m_descriptorType;
		descriptorWrite.descriptorCount = 1;

		if (m_imageResources[write.m_resourceIndex])
		{
			VkDescriptorImageInfo &imageInfo = infos[writeCount].m_imageInfo;
			imageInfo.sampler = write.m_sampler;
			imageInfo.imageView = m_imageViews[write.m_resourceIndex];
			imageInfo.imageLayout = write.m_imageLayout;

			descriptorWrite.pImageInfo = &imageInfo;
		}
		else
		{
			VkDescriptorBufferInfo &bufferInfo = infos[writeCount].m_bufferInfo;
			bufferInfo.buffer = m_buffers[write.m_resourceIndex];
			bufferInfo.offset = 0;
			bufferInfo.range = write.m_dynamicBufferSize ? write.m_dynamicBufferSize : m_resourceDescriptions[write.m_resourceIndex].m_size;

			descriptorWrite.pBufferInfo = &bufferInfo;
		}

		++writeCount;
	}

	vkUpdateDescriptorSets(g_context.m_device, writeCount, writes, 0, nullptr);
}

// pseudo code:
// for all culled passes
//		if recording command buffer and need to wait on semaphore
//			submit command buffer
//
//		if command buffer is null
//			save wait semaphores
//			create new command buffer
//			start recording
//
//		fill resource barrier arrays
//
//		if wait events
//			fill wait events array
//			wait on events
//
//		if no wait events and pending barriers
//			issue pipeline barrier
//
//		if graphics pass
//			start renderpass
//
//		if not clearpass
//			record commands
//		else
//			record clear commands
//
//		if graphics pass
//			end renderpass
//
//		if release resources
//			fill image / buffer barriers array
//			issue pipeline barrier
//
//		if end event
//			signal end event
//
//		if need to signal semaphores
//			end command buffer and submit
//
// update external resource layouts
void Graph::recordAndSubmit(size_t *firstResourceUses, size_t *lastResourceUses, SyncBits *syncBits, ResourceHandle finalResourceHandle)
{
	// we can overlap recording of different command buffers, so we have one for each queue
	VkCommandBuffer graphicsCmdBuf = VK_NULL_HANDLE;
	VkCommandBuffer computeCmdBuf = VK_NULL_HANDLE;
	VkCommandBuffer transferCmdBuf = VK_NULL_HANDLE;
	VkCommandBuffer dummyCmdBuf = VK_NULL_HANDLE;

	// multiple passes can be recorded into one command buffer, so save wait semaphores here
	struct
	{
		size_t m_count = 0;
		VkSemaphore m_waitSemaphores[MAX_WAIT_SEMAPHORES] = {};
		VkPipelineStageFlags m_waitDstStageMask = 0;
	} waitSemaphoreData[3];

	// helper lambda ends cmdBuf and submits it to queue
	auto submit = [](VkQueue queue, uint32_t waitSemaphoreCount, VkSemaphore *waitSemaphores, VkPipelineStageFlags stageMask,
		VkCommandBuffer cmdBuf, uint32_t signalSemaphoreCount, VkSemaphore *signalSemaphores, VkFence fence)
	{
		vkEndCommandBuffer(cmdBuf);

		VkPipelineStageFlags waitDstStageMask[MAX_WAIT_SEMAPHORES];
		for (size_t i = 0; i < waitSemaphoreCount; ++i)
		{
			waitDstStageMask[i] = stageMask;
		}

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = waitSemaphoreCount;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitDstStageMask;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;
		submitInfo.signalSemaphoreCount = signalSemaphoreCount;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkQueueSubmit(queue, 1, &submitInfo, fence);
	};

	for (size_t passIndex = 0; passIndex < m_passCount; ++passIndex)
	{
		// skip culled passes
		if (!m_culledPasses[passIndex])
		{
			continue;
		}

		VkCommandBuffer *cmdBuf = &dummyCmdBuf;
		VkQueue queue = VK_NULL_HANDLE;
		size_t waitSemaphoresIndex = 0;

		// find queue and cmd buf
		switch (m_queueType[passIndex])
		{
		case QueueType::GRAPHICS:
			queue = g_context.m_graphicsQueue;
			cmdBuf = &graphicsCmdBuf;
			waitSemaphoresIndex = 0;
			break;
		case QueueType::COMPUTE:
			queue = g_context.m_computeQueue;
			cmdBuf = &computeCmdBuf;
			waitSemaphoresIndex = 1;
			break;
		case QueueType::TRANSFER:
			queue = g_context.m_transferQueue;
			cmdBuf = &transferCmdBuf;
			waitSemaphoresIndex = 2;
			break;
		case QueueType::NONE:
			break;
		default:
			assert(false);
			break;
		}

		const auto &sync = syncBits[passIndex];

		// if we are recording and need to wait, end recording and submit
		if (*cmdBuf != VK_NULL_HANDLE && sync.m_waitSemaphoreCount)
		{
			submit(queue, waitSemaphoreData[waitSemaphoresIndex].m_count, waitSemaphoreData[waitSemaphoresIndex].m_waitSemaphores,
				waitSemaphoreData[waitSemaphoresIndex].m_waitDstStageMask, *cmdBuf, 0, nullptr, VK_NULL_HANDLE);
			waitSemaphoreData[waitSemaphoresIndex].m_count = 0;
			waitSemaphoreData[waitSemaphoresIndex].m_waitDstStageMask = 0;

			*cmdBuf = VK_NULL_HANDLE;
		}

		// if we are currently not recording, create a new cmd buf and start
		if (*cmdBuf == VK_NULL_HANDLE && m_queueType[passIndex] != QueueType::NONE)
		{
			// save wait semaphores
			if (sync.m_waitSemaphoreCount)
			{
				size_t remaining = sync.m_waitSemaphoreCount;
				size_t index = 0;

				auto &semaphoreData = waitSemaphoreData[waitSemaphoresIndex];

				while (remaining)
				{
					if (sync.m_waitSemaphores[index])
					{
						semaphoreData.m_waitSemaphores[semaphoreData.m_count++] = m_semaphores[index];
						--remaining;
					}
					++index;
				}

				semaphoreData.m_waitDstStageMask = m_passStageMasks[passIndex];
			}

			// get new command buffer
			switch (m_queueType[passIndex])
			{
			case QueueType::GRAPHICS:
				*cmdBuf = m_commandBuffers[0][m_graphicsCommandBufferCount++];
				break;
			case QueueType::COMPUTE:
				*cmdBuf = m_commandBuffers[1][m_computeCommandBufferCount++];
				break;
			case QueueType::TRANSFER:
				*cmdBuf = m_commandBuffers[2][m_transferCommandBufferCount++];
				break;
			case QueueType::NONE:
				assert(false);
				break;
			default:
				assert(false);
				break;
			}

			// start recording
			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(*cmdBuf, &beginInfo);
		}

		// handle resource barriers
		{
			VkImageMemoryBarrier imageBarriers[MAX_RESOURCE_BARRIERS];
			VkBufferMemoryBarrier bufferBarriers[MAX_RESOURCE_BARRIERS];
			size_t imageBarrierCount = 0;
			size_t bufferBarrierCount = 0;

			// fill resource barriers arrays
			{
				size_t remaining = sync.m_resourceBarrierCount;
				size_t resourceIndex = 0;

				while (remaining)
				{
					if (sync.m_barrierResources[resourceIndex])
					{
						size_t previousPassIndex = ContainerUtility::findPreviousSetBit(m_accessedResources[resourceIndex], passIndex);
						bool isFirstUse = previousPassIndex == passIndex;
						bool concurrent = m_concurrentResources[resourceIndex];

						if (m_imageResources[resourceIndex])
						{
							VkImageLayout oldImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
							VkImageLayout newImageLayout = m_resourceUsages[resourceIndex][passIndex].m_imageLayout;

							if (isFirstUse)
							{
								oldImageLayout = m_externalResources[resourceIndex] ? *m_externalLayouts[resourceIndex] : VK_IMAGE_LAYOUT_UNDEFINED;
							}
							else
							{
								oldImageLayout = m_resourceUsages[resourceIndex][previousPassIndex].m_imageLayout;;
							}

							VkImageMemoryBarrier &imageBarrier = imageBarriers[imageBarrierCount++];
							imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
							imageBarrier.srcAccessMask = isFirstUse ? 0 : m_resourceUsages[resourceIndex][previousPassIndex].m_accessMask;
							imageBarrier.dstAccessMask = m_resourceUsages[resourceIndex][passIndex].m_accessMask;
							imageBarrier.oldLayout = oldImageLayout;
							imageBarrier.newLayout = newImageLayout;
							imageBarrier.srcQueueFamilyIndex = concurrent ? VK_QUEUE_FAMILY_IGNORED : queueIndexFromQueueType(m_queueType[previousPassIndex]);
							imageBarrier.dstQueueFamilyIndex = concurrent ? VK_QUEUE_FAMILY_IGNORED : queueIndexFromQueueType(m_queueType[passIndex]);
							imageBarrier.image = m_images[resourceIndex];
							imageBarrier.subresourceRange = { VKUtility::imageAspectMaskFromFormat(m_resourceDescriptions[resourceIndex].m_format), 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
						}
						else
						{
							VkBufferMemoryBarrier &bufferBarrier = bufferBarriers[bufferBarrierCount++];
							bufferBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
							bufferBarrier.srcAccessMask = isFirstUse ? 0 : m_resourceUsages[resourceIndex][previousPassIndex].m_accessMask;
							bufferBarrier.dstAccessMask = m_resourceUsages[resourceIndex][passIndex].m_accessMask;
							bufferBarrier.srcQueueFamilyIndex = concurrent ? VK_QUEUE_FAMILY_IGNORED : queueIndexFromQueueType(m_queueType[previousPassIndex]);
							bufferBarrier.dstQueueFamilyIndex = concurrent ? VK_QUEUE_FAMILY_IGNORED : queueIndexFromQueueType(m_queueType[passIndex]);
							bufferBarrier.buffer = m_buffers[resourceIndex];
							bufferBarrier.offset = 0;
							bufferBarrier.size = m_resourceDescriptions[resourceIndex].m_size;
						}
						--remaining;
					}
					++resourceIndex;
				}
			}

			// wait for events
			if (sync.m_waitEventCount)
			{
				VkEvent events[MAX_WAIT_EVENTS];

				// fill events array
				{
					size_t waitEventCount = 0;
					size_t remaining = sync.m_waitEventCount;
					size_t index = 0;

					while (remaining)
					{
						if (sync.m_waitEvents[index])
						{
							events[waitEventCount++] = m_events[index];
							--remaining;
						}
						++index;
					}
				}

				VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
				memoryBarrier.srcAccessMask = sync.m_memoryBarrierSrcAccessMask;
				memoryBarrier.dstAccessMask = sync.m_memoryBarrierDstAccessMask;

				vkCmdWaitEvents(*cmdBuf, sync.m_waitEventCount, events, sync.m_waitEventsSrcStageMask, m_passStageMasks[passIndex], 1, &memoryBarrier, bufferBarrierCount, bufferBarriers, imageBarrierCount, imageBarriers);
			}

			// if no events to wait on and we still have pending resource barriers, issue a pipeline barrier
			if (!sync.m_waitEventCount && (bufferBarrierCount || imageBarrierCount))
			{
				vkCmdPipelineBarrier(*cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_passStageMasks[passIndex], 0, 0, nullptr, bufferBarrierCount, bufferBarriers, imageBarrierCount, imageBarriers);
			}
		}

		// start renderpass
		if (m_passTypes[passIndex] == PassType::GRAPHICS)
		{
			VkClearValue clearValues[MAX_COLOR_ATTACHMENTS + 1];
			size_t attachmentCount = 0;

			// fill clearValues array
			for (size_t i = 0; i < MAX_COLOR_ATTACHMENTS; ++i)
			{
				auto handle = m_framebufferInfo[passIndex].m_colorAttachments[i];
				size_t resourceIndex = (size_t)handle - 1;
				if (handle
					&& firstResourceUses[resourceIndex] == passIndex
					&& m_clearResources[resourceIndex])
				{
					clearValues[i] = m_clearValues[resourceIndex].m_imageClearValue;
				}

				if (handle)
				{
					++attachmentCount;
				}
			}

			if (m_framebufferInfo[passIndex].m_depthStencilAttachment)
			{
				clearValues[attachmentCount] = m_clearValues[(size_t)m_framebufferInfo[passIndex].m_depthStencilAttachment-1].m_imageClearValue;
			}


			VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = m_renderpassFramebufferHandles[passIndex].first;
			renderPassInfo.framebuffer = m_renderpassFramebufferHandles[passIndex].second;
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = { m_framebufferInfo[passIndex].m_width, m_framebufferInfo[passIndex].m_height };
			renderPassInfo.clearValueCount = MAX_COLOR_ATTACHMENTS + 1;
			renderPassInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(*cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		}

		// record commands
		if (m_passTypes[passIndex] != PassType::CLEAR)
		{
			m_passes[passIndex]->record(*cmdBuf, ResourceRegistry(m_images, m_imageViews, m_buffers, m_allocations));
		}
		// clear pass
		else
		{
			for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
			{
				if (m_accessedResources[resourceIndex][passIndex])
				{
					// is resource image?
					if (m_imageResources[resourceIndex])
					{
						VkImage image = m_images[resourceIndex];
						VkFormat format = m_resourceDescriptions[resourceIndex].m_format;
						VkImageSubresourceRange subresourceRange =
						{
							VKUtility::imageAspectMaskFromFormat(format),
							0,
							VK_REMAINING_MIP_LEVELS,
							0,
							1,
						};

						if (VKUtility::isDepthFormat(format))
						{
							vkCmdClearDepthStencilImage(*cmdBuf, image, m_resourceUsages[resourceIndex][passIndex].m_imageLayout,
								&m_clearValues[resourceIndex].m_imageClearValue.depthStencil, 1, &subresourceRange);
						}
						else
						{
							vkCmdClearColorImage(*cmdBuf, image, m_resourceUsages[resourceIndex][passIndex].m_imageLayout,
								&m_clearValues[resourceIndex].m_imageClearValue.color, 1, &subresourceRange);
						}
					}
					else
					{
						vkCmdFillBuffer(*cmdBuf, m_buffers[resourceIndex], 0, m_resourceDescriptions[resourceIndex].m_size, m_clearValues[resourceIndex].m_bufferClearValue);
					}
				}
			}
		}

		// end renderpass
		if (m_passTypes[passIndex] == PassType::GRAPHICS)
		{
			vkCmdEndRenderPass(*cmdBuf);
		}

		// release resources
		if (sync.m_releaseCount)
		{
			size_t remaining = sync.m_releaseCount;
			size_t resourceIndex = 0;

			VkImageMemoryBarrier imageBarriers[MAX_RESOURCE_BARRIERS];
			VkBufferMemoryBarrier bufferBarriers[MAX_RESOURCE_BARRIERS];

			size_t imageBarrierCount = 0;
			size_t bufferBarrierCount = 0;

			VkPipelineStageFlags dstStageMask = 0;

			while (remaining)
			{
				if (sync.m_releaseResources[resourceIndex])
				{
					size_t nextPassIndex = ContainerUtility::findNextSetBit(m_accessedResources[resourceIndex], passIndex);
					dstStageMask |= m_passStageMasks[nextPassIndex];

					if (m_imageResources[resourceIndex])
					{
						VkImageLayout oldImageLayout = m_resourceUsages[resourceIndex][passIndex].m_imageLayout;
						// keep layout if next pass is graphics pass, as transition is handled by renderpass
						VkImageLayout newImageLayout = m_passTypes[nextPassIndex] == PassType::GRAPHICS ? oldImageLayout : m_resourceUsages[resourceIndex][nextPassIndex].m_imageLayout;

						VkImageMemoryBarrier &imageBarrier = imageBarriers[imageBarrierCount++];
						imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
						imageBarrier.srcAccessMask = m_resourceUsages[resourceIndex][passIndex].m_accessMask;
						imageBarrier.dstAccessMask = m_resourceUsages[resourceIndex][nextPassIndex].m_accessMask;
						imageBarrier.oldLayout = oldImageLayout;
						imageBarrier.newLayout = newImageLayout;
						imageBarrier.srcQueueFamilyIndex = queueIndexFromQueueType(m_queueType[passIndex]);
						imageBarrier.dstQueueFamilyIndex = queueIndexFromQueueType(m_queueType[nextPassIndex]);
						imageBarrier.image = m_images[resourceIndex];
						imageBarrier.subresourceRange = { VKUtility::imageAspectMaskFromFormat(m_resourceDescriptions[resourceIndex].m_format), 0, VK_REMAINING_MIP_LEVELS, 0, 1 };
					}
					else
					{
						VkBufferMemoryBarrier &bufferBarrier = bufferBarriers[bufferBarrierCount++];
						bufferBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
						bufferBarrier.srcAccessMask = m_resourceUsages[resourceIndex][passIndex].m_accessMask;
						bufferBarrier.dstAccessMask = m_resourceUsages[resourceIndex][nextPassIndex].m_accessMask;
						bufferBarrier.srcQueueFamilyIndex = queueIndexFromQueueType(m_queueType[passIndex]);
						bufferBarrier.dstQueueFamilyIndex = queueIndexFromQueueType(m_queueType[nextPassIndex]);
						bufferBarrier.buffer = m_buffers[resourceIndex];
						bufferBarrier.offset = 0;
						bufferBarrier.size = m_resourceDescriptions[resourceIndex].m_size;
					}
					--remaining;
				}
				++resourceIndex;
			}

			vkCmdPipelineBarrier(*cmdBuf, m_passStageMasks[passIndex], dstStageMask, 0, 0, nullptr, bufferBarrierCount, bufferBarriers, imageBarrierCount, imageBarriers);
		}

		// signal end event
		if (m_events[passIndex] != VK_NULL_HANDLE)
		{
			vkCmdSetEvent(*cmdBuf, m_events[passIndex], m_passStageMasks[passIndex]);
		}

		// if we need to signal, end command buffer and submit
		if (sync.m_signalSemaphoreCount)
		{
			VkSemaphore signalSemaphores[MAX_SIGNAL_SEMAPHORES];

			size_t remaining = sync.m_signalSemaphoreCount;
			size_t index = 0;

			while (remaining)
			{
				if (sync.m_signalSemaphores[index])
				{
					signalSemaphores[sync.m_signalSemaphoreCount - remaining] = m_semaphores[index];
					--remaining;
				}
				++index;
			}

			VkFence fence = VK_NULL_HANDLE;

			if (lastResourceUses[(size_t)finalResourceHandle - 1] == passIndex)
			{
				assert(m_fence == VK_NULL_HANDLE);
				m_fence = fence = m_syncPrimitiveAllocator.acquireFence();
			}

			if (m_queueType[passIndex] != QueueType::NONE)
			{
				submit(queue, waitSemaphoreData[waitSemaphoresIndex].m_count, waitSemaphoreData[waitSemaphoresIndex].m_waitSemaphores,
					waitSemaphoreData[waitSemaphoresIndex].m_waitDstStageMask, *cmdBuf, sync.m_signalSemaphoreCount, signalSemaphores, fence);
				waitSemaphoreData[waitSemaphoresIndex].m_count = 0;
				waitSemaphoreData[waitSemaphoresIndex].m_waitDstStageMask = 0;

				*cmdBuf = VK_NULL_HANDLE;
			}
		}
	}

	// update external resources image layouts
	for (size_t resourceIndex = 0; resourceIndex < m_resourceCount; ++resourceIndex)
	{
		if (m_externalResources[resourceIndex])
		{
			*m_externalLayouts[resourceIndex] = m_resourceUsages[resourceIndex][lastResourceUses[resourceIndex]].m_imageLayout;
		}
	}

	// we should have submitted all cmdBufs at this point
	VkCommandBuffer cmdBufs[] = { graphicsCmdBuf, computeCmdBuf, transferCmdBuf };

	for (size_t i = 0; i < 3; ++i)
	{
		assert(cmdBufs[i] == VK_NULL_HANDLE);
	}
}

uint32_t Graph::queueIndexFromQueueType(QueueType queueType)
{
	switch (queueType)
	{
	case QueueType::GRAPHICS:
		return g_context.m_queueFamilyIndices.m_graphicsFamily;
	case QueueType::COMPUTE:
		return g_context.m_queueFamilyIndices.m_computeFamily;
	case QueueType::TRANSFER:
		return g_context.m_queueFamilyIndices.m_transferFamily;
	default:
		assert(false);
		break;
	}
	return 0;
}

void Graph::addDescriptorWrite(size_t passIndex, size_t resourceIndex, VkDescriptorSet set, uint32_t binding, uint32_t arrayElement, VkDescriptorType type, VkImageLayout imageLayout, VkSampler sampler, VkDeviceSize dynamicBufferSize)
{
	size_t descriptorSetIndex = 0;

	// find descriptor set index
	bool foundExistingSet = false;
	for (size_t i = 0; i < m_descriptorSetCount; ++i)
	{
		if (m_descriptorSets[i] == set)
		{
			foundExistingSet = true;
			descriptorSetIndex = i;
			break;
		}
	}

	if (!foundExistingSet)
	{
		assert(m_descriptorSetCount < MAX_DESCRIPTOR_SETS);
		descriptorSetIndex = m_descriptorSetCount++;
	}

	m_descriptorSets[descriptorSetIndex] = set;

	m_accessedDescriptorSets[descriptorSetIndex][passIndex] = true;

	bool foundExistingWrite = false;

	// look for existing write
	for (size_t i = 0; i < m_descriptorWriteCount; ++i)
	{
		const auto &write = m_descriptorWrites[i];

		// found existing
		if (write.m_descriptorSetIndex == descriptorSetIndex
			&& write.m_binding == binding
			&& write.m_arrayIndex == arrayElement)
		{
			// write must be identical
			assert(write.m_resourceIndex == resourceIndex);
			assert(write.m_sampler == sampler);
			assert(write.m_imageLayout == imageLayout);
			assert(write.m_dynamicBufferSize == dynamicBufferSize);

			foundExistingWrite = true;
			break;
		}
	}

	// if no existing write, add a new one
	if (!foundExistingWrite)
	{
		assert(m_descriptorWriteCount < MAX_DESCRIPTOR_WRITES);

		auto &write = m_descriptorWrites[m_descriptorWriteCount++];
		write.m_descriptorSetIndex = descriptorSetIndex;
		write.m_resourceIndex = resourceIndex;
		write.m_binding = binding;
		write.m_arrayIndex = arrayElement;
		write.m_descriptorType = type;
		write.m_sampler = sampler;
		write.m_imageLayout = imageLayout;
		write.m_dynamicBufferSize = dynamicBufferSize;
	}
}

VEngine::FrameGraph::ResourceRegistry::ResourceRegistry(const VkImage *images, const VkImageView *views, const VkBuffer *buffers, const VEngine::VKAllocationHandle *allocations)
	:m_images(images),
	m_imageViews(views),
	m_buffers(buffers),
	m_allocations(allocations)
{
}

VkImage ResourceRegistry::getImage(ImageHandle handle) const
{
	return m_images[(size_t)handle - 1];
}

VkImageView ResourceRegistry::getImageView(ImageHandle handle) const
{
	return m_imageViews[(size_t)handle - 1];
}

VkBuffer ResourceRegistry::getBuffer(BufferHandle handle) const
{
	return m_buffers[(size_t)handle - 1];
}

const VEngine::VKAllocationHandle &ResourceRegistry::getAllocation(ResourceHandle handle) const
{
	return m_allocations[(size_t)handle - 1];
}

const VEngine::VKAllocationHandle &ResourceRegistry::getAllocation(ImageHandle handle) const
{
	return m_allocations[(size_t)handle - 1];
}

const VEngine::VKAllocationHandle &ResourceRegistry::getAllocation(BufferHandle handle) const
{
	return m_allocations[(size_t)handle - 1];
}

void *ResourceRegistry::mapMemory(BufferHandle handle) const
{
	void *data;
	g_context.m_allocator.mapMemory(m_allocations[(size_t)handle - 1], &data);
	return data;
}

void ResourceRegistry::unmapMemory(BufferHandle handle) const
{
	g_context.m_allocator.unmapMemory(m_allocations[(size_t)handle - 1]);
}
