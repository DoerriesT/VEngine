#include "FrameGraph.h"
#include <cassert>
#include <stack>
#include <algorithm>
#include "Utility/ContainerUtility.h"
#include "Utility/Utility.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKUtility.h"
#include <GLFW/glfw3.h>
#include <iostream>

using namespace VEngine::FrameGraph;

PassBuilder::PassBuilder(Graph &graph, Pass &pass, uint32_t passIndex)
	:m_graph(graph),
	m_pass(pass),
	m_passIndex(passIndex)
{
}

void PassBuilder::setDimensions(uint32_t width, uint32_t height)
{
	m_pass.m_width = width;
	m_pass.m_height = height;
}

void PassBuilder::readDepthStencil(ImageHandle handle)
{
	assert(!ContainerUtility::contains(m_pass.m_readImages, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeImages, handle));

	ImageResourceStage resourceStage =
	{
		m_passIndex,
		ImageAccessType::DEPTH_STENCIL,
		false,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_readImages.push_back(handle);
	m_pass.m_depthStencilAttachment = handle;
}

void PassBuilder::readInputAttachment(ImageHandle handle, uint32_t index)
{
	assert(!ContainerUtility::contains(m_pass.m_readImages, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeImages, handle));

	ImageResourceStage resourceStage =
	{
		m_passIndex,
		ImageAccessType::TEXTURE_LOCAL,
		false,
		VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_readImages.push_back(handle);
}

void PassBuilder::readTexture(ImageHandle handle, VkPipelineStageFlags stageFlags, bool local, uint32_t baseMipLevel, uint32_t levelCount)
{
	assert(!ContainerUtility::contains(m_pass.m_readImages, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeImages, handle));

	ImageResourceStage resourceStage =
	{
		m_passIndex,
		ImageAccessType::TEXTURE,
		false,
		VK_IMAGE_USAGE_SAMPLED_BIT,
		stageFlags,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_readImages.push_back(handle);
}

void PassBuilder::readStorageImage(ImageHandle handle, VkPipelineStageFlags stageFlags, bool local)
{
	assert(!ContainerUtility::contains(m_pass.m_readImages, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeImages, handle));

	ImageResourceStage resourceStage =
	{
		m_passIndex,
		ImageAccessType::STORAGE_IMAGE,
		false,
		VK_IMAGE_USAGE_STORAGE_BIT,
		stageFlags,
		VK_ACCESS_SHADER_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_readImages.push_back(handle);
}

void PassBuilder::readStorageBuffer(BufferHandle handle, VkPipelineStageFlags stageFlags)
{
	assert(!ContainerUtility::contains(m_pass.m_readBuffers, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeBuffers, handle));

	BufferResourceStage resourceStage =
	{
		m_passIndex,
		BufferAccessType::STORAGE_BUFFER,
		false,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		stageFlags,
		VK_ACCESS_SHADER_READ_BIT
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_readBuffers.push_back(handle);
}

void PassBuilder::readUniformBuffer(BufferHandle handle, VkPipelineStageFlags stageFlags)
{
	assert(!ContainerUtility::contains(m_pass.m_readBuffers, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeBuffers, handle));

	BufferResourceStage resourceStage =
	{
		m_passIndex,
		BufferAccessType::UNIFORM_BUFFER,
		false,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		stageFlags,
		VK_ACCESS_UNIFORM_READ_BIT
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_readBuffers.push_back(handle);
}

void PassBuilder::readVertexBuffer(BufferHandle handle)
{
	assert(!ContainerUtility::contains(m_pass.m_readBuffers, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeBuffers, handle));

	BufferResourceStage resourceStage =
	{
		m_passIndex,
		BufferAccessType::VERTEX_BUFFER,
		false,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_readBuffers.push_back(handle);
}

void PassBuilder::readIndexBuffer(BufferHandle handle)
{
	assert(!ContainerUtility::contains(m_pass.m_readBuffers, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeBuffers, handle));

	BufferResourceStage resourceStage =
	{
		m_passIndex,
		BufferAccessType::INDEX_BUFFER,
		false,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
		VK_ACCESS_INDEX_READ_BIT
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_readBuffers.push_back(handle);
}

void PassBuilder::readIndirectBuffer(BufferHandle handle)
{
	assert(!ContainerUtility::contains(m_pass.m_readBuffers, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeBuffers, handle));

	BufferResourceStage resourceStage =
	{
		m_passIndex,
		BufferAccessType::INDIRECT_BUFFER,
		false,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
		VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
		VK_ACCESS_INDIRECT_COMMAND_READ_BIT
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_readBuffers.push_back(handle);
}

void PassBuilder::writeDepthStencil(ImageHandle handle)
{
	assert(!ContainerUtility::contains(m_pass.m_readImages, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeImages, handle));

	ImageResourceStage resourceStage =
	{
		m_passIndex,
		ImageAccessType::DEPTH_STENCIL,
		true,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_writeImages.push_back(handle);
	m_pass.m_depthStencilAttachment = handle;
}

void PassBuilder::writeColorAttachment(ImageHandle handle, uint32_t index, bool read)
{
	assert(!ContainerUtility::contains(m_pass.m_readImages, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeImages, handle));

	ImageResourceStage resourceStage =
	{
		m_passIndex,
		ImageAccessType::TEXTURE,
		true,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_writeImages.push_back(handle);
	if (m_pass.m_colorAttachments.size() < index + 1)
	{
		m_pass.m_colorAttachments.resize(index + 1);
	}
	m_pass.m_colorAttachments[index] = handle;
}

void PassBuilder::writeStorageImage(ImageHandle handle, VkPipelineStageFlags stageFlags)
{
	assert(!ContainerUtility::contains(m_pass.m_readImages, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeImages, handle));

	ImageResourceStage resourceStage =
	{
		m_passIndex,
		ImageAccessType::TEXTURE,
		true,
		VK_IMAGE_USAGE_STORAGE_BIT,
		stageFlags,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_IMAGE_LAYOUT_GENERAL
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_writeImages.push_back(handle);
}

void PassBuilder::writeStorageBuffer(BufferHandle handle, VkPipelineStageFlags stageFlags)
{
	assert(!ContainerUtility::contains(m_pass.m_readBuffers, handle));
	assert(!ContainerUtility::contains(m_pass.m_writeBuffers, handle));

	BufferResourceStage resourceStage =
	{
		m_passIndex,
		BufferAccessType::INDIRECT_BUFFER,
		true,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		stageFlags,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
	};

	m_graph.addResourceStage(handle, resourceStage);
	m_pass.m_writeBuffers.push_back(handle);
}

ImageHandle PassBuilder::createImage(const ImageDesc &imageDesc)
{
	return m_graph.createImage(imageDesc);
}

BufferHandle PassBuilder::createBuffer(const BufferDesc &bufferDesc)
{
	return m_graph.createBuffer(bufferDesc);
}

void Graph::setBackBuffer(ImageHandle handle)
{
	m_backBufferHandle = handle;
}

void Graph::setPresentParams(VkSwapchainKHR *swapChain, uint32_t swapChainImageIndex, QueueType queue, VkSemaphore waitSemaphore)
{
	m_swapChain = swapChain;
	m_swapChainImageIndex = swapChainImageIndex;
	m_presentQueue = queue;
	m_backBufferWaitSemaphore = waitSemaphore;
}

void Graph::addGraphicsPass(const char * name, const std::function<std::function<void(VkCommandBuffer, const ResourceRegistry&)>(PassBuilder&)>& setup)
{
	m_passes.push_back({ name, PassType::GRAPHICS });
	Pass &pass = m_passes.back();
	// fill pass with data
	PassBuilder builder(*this, pass, static_cast<uint32_t>(m_passes.size() - 1));
	pass.m_recordCommands = setup(builder);
	pass.m_queue = QueueType::GRAPHICS;
}

void Graph::addComputePass(const char * name, QueueType queue, const std::function<std::function<void(VkCommandBuffer, const ResourceRegistry&)>(PassBuilder&)>& setup)
{
	m_passes.push_back({ name, PassType::COMPUTE });
	Pass &pass = m_passes.back();
	// fill pass with data
	PassBuilder builder(*this, pass, static_cast<uint32_t>(m_passes.size() - 1));
	pass.m_recordCommands = setup(builder);
	pass.m_queue = queue;
}

void Graph::addBlitPass(const char * name, QueueType queue, ImageHandle srcHandle, ImageHandle dstHandle, const std::vector<VkImageBlit>& regions, VkFilter filter)
{
	m_passes.push_back({ name, PassType::BLIT });
	Pass &pass = m_passes.back();
	uint32_t passIndex = static_cast<uint32_t>(m_passes.size() - 1);

	// src
	{
		ImageResourceStage srcResourceStage
		{
			passIndex,
			ImageAccessType::BLIT,
			false,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		};

		addResourceStage(srcHandle, srcResourceStage);
		pass.m_readImages.push_back(srcHandle);
	}

	// dst
	{
		ImageResourceStage dstResourceStage
		{
			passIndex,
			ImageAccessType::BLIT,
			true,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		};

		addResourceStage(dstHandle, dstResourceStage);
		pass.m_writeImages.push_back(dstHandle);
	}

	pass.m_recordCommands = [=](VkCommandBuffer cmdBuf, const ResourceRegistry &registry)
	{
		vkCmdBlitImage(cmdBuf, registry.getImage(srcHandle), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, registry.getImage(dstHandle), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size(), regions.data(), filter);
	};
	pass.m_queue = queue;
}

void Graph::addResourceStage(ImageHandle handle, const ImageResourceStage &imageResourceStage)
{
	m_resourceRegistry.m_virtualImages[(size_t)handle - 1].m_stages.push_back(imageResourceStage);
}

void Graph::addResourceStage(BufferHandle handle, const BufferResourceStage &bufferResourceStage)
{
	m_resourceRegistry.m_virtualBuffers[(size_t)handle - 1].m_stages.push_back(bufferResourceStage);
}

void Graph::compile()
{
	auto total = glfwGetTime();
	auto time = glfwGetTime();
	cull();
	auto cullTime = glfwGetTime() - time;

	time = glfwGetTime();
	createResources();
	auto resourceTime = glfwGetTime() - time;

	time = glfwGetTime();
	createVirtualBarriers();
	auto virtBarriersTime = glfwGetTime() - time;

	time = glfwGetTime();
	createRenderPasses();
	auto renderPassesTime = glfwGetTime() - time;

	time = glfwGetTime();
	createPhysicalBarriers();
	auto barriersTime = glfwGetTime() - time;

	total = glfwGetTime() - total;

	std::cout << "cull " << cullTime * 1000.0 << std::endl;
	std::cout << "createResources " << resourceTime * 1000.0 << std::endl;
	std::cout << "createVirtualBarriers " << virtBarriersTime * 1000.0 << std::endl;
	std::cout << "createRenderPasses " << renderPassesTime * 1000.0 << std::endl;
	std::cout << "createPhysicalBarriers " << barriersTime * 1000.0 << std::endl;
	std::cout << "total " << (total) * 1000.0 << std::endl;

	int a = 5;
}

void Graph::execute()
{
	for (Pass &pass : m_passes)
	{
		VkCommandBuffer cmdBuf = nullptr;

		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmdBuf, &beginInfo);
		{
			// start pipeline barrier?
			if (pass.m_pipelineBarrier.m_srcStageMask != 0 && pass.m_pipelineBarrier.m_dstStageMask != 0)
			{
				vkCmdPipelineBarrier(cmdBuf,
					pass.m_pipelineBarrier.m_srcStageMask,
					pass.m_pipelineBarrier.m_dstStageMask,
					0,
					pass.m_pipelineBarrier.m_memoryBarrier.srcAccessMask != 0 && pass.m_pipelineBarrier.m_memoryBarrier.dstAccessMask != 0 ? 1 : 0,
					&pass.m_pipelineBarrier.m_memoryBarrier,
					pass.m_pipelineBarrier.m_bufferBarriers.size(),
					pass.m_pipelineBarrier.m_bufferBarriers.data(),
					pass.m_pipelineBarrier.m_imageBarriers.size(),
					pass.m_pipelineBarrier.m_imageBarriers.data());
			}

			// wait events?
			if (!pass.m_waitEvents.m_events.empty())
			{
				vkCmdWaitEvents(cmdBuf,
					pass.m_waitEvents.m_events.size(),
					pass.m_waitEvents.m_events.data(),
					pass.m_waitEvents.m_srcStageMask,
					pass.m_waitEvents.m_dstStageMask,
					pass.m_waitEvents.m_memoryBarrier.srcAccessMask != 0 && pass.m_waitEvents.m_memoryBarrier.dstAccessMask != 0 ? 1 : 0,
					&pass.m_waitEvents.m_memoryBarrier,
					pass.m_waitEvents.m_bufferBarriers.size(),
					pass.m_waitEvents.m_bufferBarriers.data(),
					pass.m_waitEvents.m_imageBarriers.size(),
					pass.m_waitEvents.m_imageBarriers.data());
			}

			// renderpass ?
			if (pass.m_passType == PassType::GRAPHICS)
			{
				VkClearValue clearValues[2] = {};
				clearValues[0].depthStencil = { 1.0f, 0 };
				clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };


				VkRenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.renderPass = pass.m_renderPassHandle;
				renderPassInfo.framebuffer = pass.m_framebufferHandle;
				renderPassInfo.renderArea.offset = { 0, 0 };
				renderPassInfo.renderArea.extent = { pass.m_width, pass.m_height };
				renderPassInfo.clearValueCount = static_cast<uint32_t>(sizeof(clearValues) / sizeof(clearValues[0]));
				renderPassInfo.pClearValues = clearValues;

				vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			}

			// record
			pass.m_recordCommands(cmdBuf, m_resourceRegistry);

			// renderpass ?
			if (pass.m_passType == PassType::GRAPHICS)
			{
				vkCmdEndRenderPass(cmdBuf);
			}

			// set event?
			if (pass.m_event != VK_NULL_HANDLE)
			{
				vkCmdSetEvent(cmdBuf, pass.m_event, pass.m_eventStageMask);
			}

			// end pipeline barrier?
			if (pass.m_endPipelineBarrier.m_srcStageMask != 0 && pass.m_endPipelineBarrier.m_dstStageMask != 0)
			{
				vkCmdPipelineBarrier(cmdBuf,
					pass.m_endPipelineBarrier.m_srcStageMask,
					pass.m_endPipelineBarrier.m_dstStageMask,
					0,
					pass.m_endPipelineBarrier.m_memoryBarrier.srcAccessMask != 0 && pass.m_endPipelineBarrier.m_memoryBarrier.dstAccessMask != 0 ? 1 : 0,
					&pass.m_endPipelineBarrier.m_memoryBarrier,
					pass.m_endPipelineBarrier.m_bufferBarriers.size(),
					pass.m_endPipelineBarrier.m_bufferBarriers.data(),
					pass.m_endPipelineBarrier.m_imageBarriers.size(),
					pass.m_endPipelineBarrier.m_imageBarriers.data());
			}
		}
		vkEndCommandBuffer(cmdBuf);
	}
}

ImageHandle Graph::createImage(const ImageDesc & imageDesc)
{
	m_resourceRegistry.m_virtualImages.push_back({ imageDesc });
	return ImageHandle(m_resourceRegistry.m_virtualImages.size());
}

BufferHandle Graph::createBuffer(const BufferDesc & bufferDesc)
{
	m_resourceRegistry.m_virtualBuffers.push_back({ bufferDesc });
	return BufferHandle(m_resourceRegistry.m_virtualBuffers.size());
}

void Graph::cull()
{
	// backbuffer handle needs to be set
	assert(m_backBufferHandle);

	// compute initial reference counts
	for (Pass &pass : m_passes)
	{
		pass.m_refCount += pass.m_writeImages.size() + pass.m_writeBuffers.size();

		for (auto handle : pass.m_readImages)
		{
			++m_resourceRegistry.m_virtualImages[(size_t)handle - 1].m_refCount;
		}

		for (auto handle : pass.m_readBuffers)
		{
			++m_resourceRegistry.m_virtualBuffers[(size_t)handle - 1].m_refCount;
		}
	}

	// increment ref count on backbuffer handle as it is "read" when presenting
	++m_resourceRegistry.m_virtualImages[(size_t)m_backBufferHandle - 1].m_refCount;

	// fill stacks with resources with refCount == 0
	std::stack<ImageHandle> imageStack;
	std::stack<BufferHandle> bufferStack;

	for (size_t i = 0; i < m_resourceRegistry.m_virtualImages.size(); ++i)
	{
		if (m_resourceRegistry.m_virtualImages[i].m_refCount == 0)
		{
			imageStack.push(ImageHandle(i + 1));
		}
	}

	for (size_t i = 0; i < m_resourceRegistry.m_virtualBuffers.size(); ++i)
	{
		if (m_resourceRegistry.m_virtualBuffers[i].m_refCount == 0)
		{
			bufferStack.push(BufferHandle(i + 1));
		}
	}

	// helper lambda to reduce code duplication
	auto decrement = [this, &imageStack, &bufferStack](Pass &pass)
	{
		// decrement refCount. if it falls to zero, decrement refcount of read resources
		if ((pass.m_refCount != 0) && (--pass.m_refCount == 0))
		{
			for (auto handle : pass.m_readImages)
			{
				auto &readImg = m_resourceRegistry.m_virtualImages[(size_t)handle - 1];

				// decrement resource refCount. if it falls to zero, add it to stack
				if ((readImg.m_refCount != 0) && (--readImg.m_refCount == 0))
				{
					imageStack.push(handle);
				}
			}

			for (auto handle : pass.m_readBuffers)
			{
				auto &readBuf = m_resourceRegistry.m_virtualBuffers[(size_t)handle - 1];

				// decrement resource refCount. if it falls to zero, add it to stack
				if ((readBuf.m_refCount != 0) && (--readBuf.m_refCount == 0))
				{
					bufferStack.push(handle);
				}
			}
		}
	};

	// cull passes/resources
	while (!imageStack.empty() || !bufferStack.empty())
	{
		// images
		if (!imageStack.empty())
		{
			VirtualImage &img = m_resourceRegistry.m_virtualImages[(size_t)imageStack.top() - 1];
			imageStack.pop();

			// find writing passes
			for (size_t i = 0; i < img.m_stages.size(); ++i)
			{
				if (img.m_stages[i].m_write)
				{
					decrement(m_passes[img.m_stages[i].m_passIndex]);
				}
			}
		}

		// buffers
		if (!bufferStack.empty())
		{
			VirtualBuffer &buf = m_resourceRegistry.m_virtualBuffers[(size_t)bufferStack.top() - 1];
			bufferStack.pop();

			// find writing passes
			for (size_t i = 0; i < buf.m_stages.size(); ++i)
			{
				if (buf.m_stages[i].m_write)
				{
					decrement(m_passes[buf.m_stages[i].m_passIndex]);
				}
			}
		}
	}

	// ensure that resources that belong to referenced passes are not culled
	{
		for (Pass &pass : m_passes)
		{
			if (pass.m_refCount)
			{
				for (auto handle : pass.m_readImages)
				{
					auto &img = m_resourceRegistry.m_virtualImages[(size_t)handle - 1];
					img.m_refCount = std::max((size_t)1, img.m_refCount);
				}
				for (auto handle : pass.m_writeImages)
				{
					auto &img = m_resourceRegistry.m_virtualImages[(size_t)handle - 1];
					img.m_refCount = std::max((size_t)1, img.m_refCount);
				}
				for (auto handle : pass.m_readBuffers)
				{
					auto &buf = m_resourceRegistry.m_virtualBuffers[(size_t)handle - 1];
					buf.m_refCount = std::max((size_t)1, buf.m_refCount);
				}
				for (auto handle : pass.m_writeBuffers)
				{
					auto &buf = m_resourceRegistry.m_virtualBuffers[(size_t)handle - 1];
					buf.m_refCount = std::max((size_t)1, buf.m_refCount);
				}
			}
		}
	}
}

void Graph::createResources()
{
	for (auto &img : m_resourceRegistry.m_virtualImages)
	{
		if (img.m_refCount)
		{
			VkImageUsageFlags usageFlags = 0;

			// what usage flags do we need?
			for (auto &stage : img.m_stages)
			{
				Pass &pass = m_passes[stage.m_passIndex];
				if (pass.m_refCount)
				{
					usageFlags |= stage.m_usage;
				}
			}

			// create image
			VKImage image = {};
			{
				VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
				imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imageCreateInfo.format = img.m_desc.m_format;
				imageCreateInfo.extent.width = img.m_desc.m_width;
				imageCreateInfo.extent.height = img.m_desc.m_height;
				imageCreateInfo.extent.depth = 1;
				imageCreateInfo.mipLevels = img.m_desc.m_levels;
				imageCreateInfo.arrayLayers = 1;
				imageCreateInfo.samples = VkSampleCountFlagBits(img.m_desc.m_samples);
				imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.usage = usageFlags;
				imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

				VmaAllocationCreateInfo allocCreateInfo = {};
				allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;


				image.create(imageCreateInfo, allocCreateInfo);
			}

			// create image view
			VkImageView imageView;
			{
				VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				viewInfo.image = image.getImage();
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewInfo.format = image.getFormat();
				viewInfo.subresourceRange =
				{
					VKUtility::imageAspectMaskFromFormat(viewInfo.format),
					0,
					VK_REMAINING_MIP_LEVELS ,
					0,
					1
				};

				if (vkCreateImageView(g_context.m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
				{
					Utility::fatalExit("Failed to create image view!", -1);
				}
			}

			m_resourceRegistry.m_images.push_back(image);
			m_resourceRegistry.m_imageViews.push_back(imageView);
			img.m_resourceIndex = m_resourceRegistry.m_images.size() - 1;
		}
	}

	for (size_t i = 0; i < m_resourceRegistry.m_virtualBuffers.size(); ++i)
	{
		auto &buf = m_resourceRegistry.m_virtualBuffers[i];
		if (buf.m_refCount)
		{
			VkBufferUsageFlags usageFlags = 0;

			// what usage flags do we need?
			for (auto &stage : buf.m_stages)
			{
				Pass &pass = m_passes[stage.m_passIndex];
				if (pass.m_refCount)
				{
					usageFlags |= stage.m_usage;
				}
			}

			// create buffer
			VKBuffer buffer = {};
			{
				VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
				bufferCreateInfo.size = buf.m_desc.m_size;
				bufferCreateInfo.usage = usageFlags;
				bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				VmaAllocationCreateInfo allocCreateInfo = {};
				allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;


				buffer.create(bufferCreateInfo, allocCreateInfo);
			}

			m_resourceRegistry.m_buffers.push_back(buffer);
			buf.m_resourceIndex = m_resourceRegistry.m_buffers.size() - 1;
		}
	}
}

void Graph::createVirtualBarriers()
{
	// images
	for (auto &img : m_resourceRegistry.m_virtualImages)
	{
		if (img.m_refCount == 0)
		{
			continue;
		}

		ImageResourceStage *previousStage = nullptr;
		for (auto &stage : img.m_stages)
		{
			Pass &currentPass = m_passes[stage.m_passIndex];

			// skip culled passes
			if (currentPass.m_refCount == 0)
			{
				continue;
			}

			if (previousStage)
			{
				Pass &previousPass = m_passes[previousStage->m_passIndex];
				bool layoutTransition = previousStage->m_layout != stage.m_layout;
				bool ownershipTransfer = currentPass.m_queue != previousPass.m_queue;

				// layout transition? ownership transfer?
				if (layoutTransition || ownershipTransfer)
				{
					ImageBarrier imageBarrier;
					imageBarrier.m_srcStageMask = previousStage->m_stageMask;
					imageBarrier.m_dstStageMask = stage.m_stageMask;
					imageBarrier.m_srcAccessMask = previousStage->m_accessMask;
					imageBarrier.m_dstAccessMask = stage.m_accessMask;
					imageBarrier.m_oldLayout = previousStage->m_layout;
					imageBarrier.m_newLayout = stage.m_layout;
					imageBarrier.m_srcQueueFamilyIndex = queueIndexFromQueueType(previousPass.m_queue);
					imageBarrier.m_dstQueueFamilyIndex = queueIndexFromQueueType(currentPass.m_queue);
					imageBarrier.m_image = m_resourceRegistry.m_images[img.m_resourceIndex].getImage();
					imageBarrier.m_subresourceRange =
					{
						VKUtility::imageAspectMaskFromFormat(m_resourceRegistry.m_images[img.m_resourceIndex].getFormat()),
						0,
						VK_REMAINING_MIP_LEVELS ,
						0,
						1
					};

					// add dependency
					currentPass.m_imageBarriers.push_back({ previousStage->m_passIndex, imageBarrier });
				}
				// (write - read/write) ? -> global memory barrier
				else if (previousStage->m_write)
				{
					MemoryBarrier memoryBarrier;
					memoryBarrier.m_srcStageMask = previousStage->m_stageMask;
					memoryBarrier.m_dstStageMask = stage.m_stageMask;
					memoryBarrier.m_srcAccessMask = previousStage->m_accessMask;
					memoryBarrier.m_dstAccessMask = stage.m_accessMask;

					// add dependency
					currentPass.m_memoryBarriers.push_back({ previousStage->m_passIndex, memoryBarrier });
				}
				// (read - write) ? -> execution barrier
				else if (!previousStage->m_write && stage.m_write)
				{
					ExecutionBarrier executionBarrier;
					executionBarrier.m_srcStageMask = previousStage->m_stageMask;
					executionBarrier.m_dstStageMask = stage.m_stageMask;

					// add dependency
					currentPass.m_executionBarriers.push_back({ previousStage->m_passIndex, executionBarrier });
				}
			}
			previousStage = &stage;
		}
	}

	// buffers
	for (auto &buf : m_resourceRegistry.m_virtualBuffers)
	{
		if (buf.m_refCount == 0)
		{
			continue;
		}

		BufferResourceStage *previousStage = nullptr;
		for (auto &stage : buf.m_stages)
		{
			Pass &currentPass = m_passes[stage.m_passIndex];

			// skip culled passes
			if (currentPass.m_refCount == 0)
			{
				continue;
			}

			if (previousStage)
			{
				Pass &previousPass = m_passes[previousStage->m_passIndex];
				bool ownershipTransfer = currentPass.m_queue != previousPass.m_queue;

				// ownership transfer?
				if (ownershipTransfer)
				{
					BufferBarrier bufferBarrier;
					bufferBarrier.m_srcStageMask = previousStage->m_stageMask;
					bufferBarrier.m_dstStageMask = stage.m_stageMask;
					bufferBarrier.m_srcAccessMask = previousStage->m_accessMask;
					bufferBarrier.m_dstAccessMask = stage.m_accessMask;
					bufferBarrier.m_srcQueueFamilyIndex = queueIndexFromQueueType(previousPass.m_queue);
					bufferBarrier.m_dstQueueFamilyIndex = queueIndexFromQueueType(currentPass.m_queue);
					bufferBarrier.m_buffer = m_resourceRegistry.m_buffers[buf.m_resourceIndex].getBuffer();
					bufferBarrier.m_offset = 0;
					bufferBarrier.m_size = m_resourceRegistry.m_buffers[buf.m_resourceIndex].getSize();

					// add dependency
					currentPass.m_bufferBarriers.push_back({ previousStage->m_passIndex, bufferBarrier });
				}
				// (write - read) ? -> global memory barrier
				else if (previousStage->m_write && !stage.m_write)
				{
					MemoryBarrier memoryBarrier;
					memoryBarrier.m_srcStageMask = previousStage->m_stageMask;
					memoryBarrier.m_dstStageMask = stage.m_stageMask;
					memoryBarrier.m_srcAccessMask = previousStage->m_accessMask;
					memoryBarrier.m_dstAccessMask = stage.m_accessMask;

					// add dependency
					currentPass.m_memoryBarriers.push_back({ previousStage->m_passIndex, memoryBarrier });
				}
				// (read - write) ? -> execution barrier
				else if (!previousStage->m_write && stage.m_write)
				{
					ExecutionBarrier executionBarrier;
					executionBarrier.m_srcStageMask = previousStage->m_stageMask;
					executionBarrier.m_dstStageMask = stage.m_stageMask;

					// add dependency
					currentPass.m_executionBarriers.push_back({ previousStage->m_passIndex, executionBarrier });
				}
			}
			previousStage = &stage;
		}
	}
}

void Graph::createRenderPasses()
{
	size_t previousPassIndex = 0;
	for (size_t passIndex = 0; passIndex < m_passes.size(); ++passIndex)
	{
		Pass &pass = m_passes[passIndex];

		if (pass.m_refCount == 0)
		{
			continue;
		}
		if (pass.m_passType != PassType::GRAPHICS)
		{
			previousPassIndex = passIndex;
			continue;
		}

		// required data
		std::vector<VkAttachmentDescription> attachmentDescriptions(pass.m_colorAttachments.size() + (pass.m_depthStencilAttachment ? 1 : 0));
		std::vector<VkAttachmentReference> attachmentRefs(attachmentDescriptions.size());
		VkSubpassDependency dependency = { VK_SUBPASS_EXTERNAL , 0 };
		VkSubpassDescription subpass = {};
		VkRenderPass renderPassHandle;
		VkFramebuffer framebufferHandle;

		// create dependency
		if (previousPassIndex != passIndex)
		{
			Pass &previousPass = m_passes[previousPassIndex];

			if (previousPass.m_queue == QueueType::GRAPHICS)
			{
				// find back to back dependencies (other dependencies are handled externally)

				for (auto &imageBarrier : pass.m_imageBarriers)
				{
					if (imageBarrier.first == previousPassIndex)
					{
						bool imageIsAttachment = false;

						for (auto &attachmentHandle : pass.m_colorAttachments)
						{
							if (m_resourceRegistry.m_images[m_resourceRegistry.m_virtualImages[(size_t)attachmentHandle - 1].m_resourceIndex].getImage() == imageBarrier.second.m_image)
							{
								imageIsAttachment = true;
								break;
							}
						}

						if (pass.m_depthStencilAttachment)
						{
							if (m_resourceRegistry.m_images[m_resourceRegistry.m_virtualImages[(size_t)pass.m_depthStencilAttachment - 1].m_resourceIndex].getImage() == imageBarrier.second.m_image)
							{
								imageIsAttachment = true;
							}
						}

						// transitions are only done for attachments
						if (imageIsAttachment)
						{
							imageBarrier.second.m_renderPassInternal = true;

							dependency.srcStageMask |= imageBarrier.second.m_srcStageMask;
							dependency.dstStageMask |= imageBarrier.second.m_dstStageMask;
							dependency.srcAccessMask |= imageBarrier.second.m_srcAccessMask;
							dependency.dstAccessMask |= imageBarrier.second.m_dstAccessMask;
						}
					}
				}

				for (auto &memoryBarrier : pass.m_memoryBarriers)
				{
					if (memoryBarrier.first == previousPassIndex)
					{
						dependency.srcStageMask |= memoryBarrier.second.m_srcStageMask;
						dependency.dstStageMask |= memoryBarrier.second.m_dstStageMask;
						dependency.srcAccessMask |= memoryBarrier.second.m_srcAccessMask;
						dependency.dstAccessMask |= memoryBarrier.second.m_dstAccessMask;
					}
				}

				for (auto &executionBarrier : pass.m_executionBarriers)
				{
					if (executionBarrier.first == previousPassIndex)
					{
						dependency.srcStageMask |= executionBarrier.second.m_srcStageMask;
						dependency.dstStageMask |= executionBarrier.second.m_dstStageMask;
					}
				}
			}
		}

		// create attachment descriptions and refs
		for (size_t j = 0; j < attachmentDescriptions.size(); ++j)
		{
			VkAttachmentDescription &descr = attachmentDescriptions[j];
			ImageHandle imageHandle = j < pass.m_colorAttachments.size() ? pass.m_colorAttachments[j] : pass.m_depthStencilAttachment;
			VirtualImage &virtImg = m_resourceRegistry.m_virtualImages[(size_t)imageHandle - 1];

			bool isBackBuffer = imageHandle == m_backBufferHandle;
			bool firstUse = true;
			bool lastUse = true;
			VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			ImageResourceStage *currentStage = nullptr;

			// find initial and whether the image is used before or after
			for (auto &stage : virtImg.m_stages)
			{
				if (m_passes[stage.m_passIndex].m_refCount)
				{
					if (stage.m_passIndex < passIndex)
					{
						firstUse = false;
						initialLayout = stage.m_layout;
					}
					else if (stage.m_passIndex > passIndex)
					{
						lastUse = false;
						break;
					}
					else
					{
						currentStage = &stage;
					}
				}
			}

			assert(currentStage);
			VkImageLayout finalLayout = currentStage->m_layout;

			// crete attachment ref
			attachmentRefs[j] = { static_cast<uint32_t>(j), finalLayout };

			// special case if using the backbuffer
			if (lastUse && isBackBuffer)
			{
				lastUse = false;
				finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}

			assert(finalLayout != VK_IMAGE_LAYOUT_UNDEFINED);

			descr.format = virtImg.m_desc.m_format;
			descr.samples = VkSampleCountFlagBits(virtImg.m_desc.m_samples);
			descr.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			if (firstUse)
			{
				switch (virtImg.m_desc.m_initialState)
				{
				case ImageInitialState::UNDEFINED:
					descr.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					break;
				case ImageInitialState::CLEAR:
					descr.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
					break;
				default:
					assert(false);
				}
			}
			descr.storeOp = lastUse ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
			descr.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			descr.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			descr.initialLayout = initialLayout;
			descr.finalLayout = finalLayout;
		}

		// create subpass
		{
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = static_cast<uint32_t>(attachmentRefs.size()) - (pass.m_depthStencilAttachment ? 1 : 0);
			subpass.pColorAttachments = attachmentRefs.data();
			subpass.pDepthStencilAttachment = (pass.m_depthStencilAttachment ? &attachmentRefs.back() : nullptr);
		}

		// create renderpass
		{
			VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
			renderPassInfo.pAttachments = attachmentDescriptions.data();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = dependency.srcStageMask ? 1 : 0;
			renderPassInfo.pDependencies = &dependency;

			if (vkCreateRenderPass(g_context.m_device, &renderPassInfo, nullptr, &renderPassHandle) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create render pass!", -1);
			}
		}


		// create framebuffer
		{
			std::vector<VkImageView> framebufferAttachments;
			for (auto handle : pass.m_colorAttachments)
			{
				framebufferAttachments.push_back(m_resourceRegistry.m_imageViews[m_resourceRegistry.m_virtualImages[(size_t)handle - 1].m_resourceIndex]);
			}

			if (pass.m_depthStencilAttachment)
			{
				framebufferAttachments.push_back(m_resourceRegistry.m_imageViews[m_resourceRegistry.m_virtualImages[(size_t)pass.m_depthStencilAttachment - 1].m_resourceIndex]);
			}

			VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferInfo.renderPass = renderPassHandle;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(framebufferAttachments.size());
			framebufferInfo.pAttachments = framebufferAttachments.data();
			framebufferInfo.width = pass.m_width;
			framebufferInfo.height = pass.m_height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(g_context.m_device, &framebufferInfo, nullptr, &framebufferHandle) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create framebuffer!", -1);
			}
		}
		previousPassIndex = passIndex;
	}
}

void Graph::createPhysicalBarriers()
{
	// back-to-back dependencies on the same queue except for graphics-graphics (handled by renderpass) use merged pipeline barriers.
	// non-back-to-back dependencies on the same queue are handled with events.
	// inter-queue dependencies are handled with semaphores.

	size_t previousPassIndex = 0;
	for (size_t passIndex = 0; passIndex < m_passes.size(); ++passIndex)
	{
		Pass &pass = m_passes[passIndex];
		pass.m_pipelineBarrier.m_memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		pass.m_endPipelineBarrier.m_memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		pass.m_waitEvents.m_memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		pass.m_event = m_resourceManager.acquireEvent();

		if (previousPassIndex != passIndex)
		{
			Pass &previousPass = m_passes[previousPassIndex];

			for (auto &bufferBarrier : pass.m_bufferBarriers)
			{
				VkBufferMemoryBarrier vkBufferBarrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
				vkBufferBarrier.srcAccessMask = bufferBarrier.second.m_srcAccessMask;
				vkBufferBarrier.dstAccessMask = bufferBarrier.second.m_dstAccessMask;
				vkBufferBarrier.srcQueueFamilyIndex = bufferBarrier.second.m_srcQueueFamilyIndex;
				vkBufferBarrier.dstQueueFamilyIndex = bufferBarrier.second.m_dstQueueFamilyIndex;
				vkBufferBarrier.buffer = bufferBarrier.second.m_buffer;
				vkBufferBarrier.offset = bufferBarrier.second.m_offset;
				vkBufferBarrier.size = bufferBarrier.second.m_size;

				bool interQueueDependency = pass.m_queue != m_passes[bufferBarrier.first].m_queue;

				if (interQueueDependency)
				{
					// semaphore
					Pass &dependencyPass = m_passes[bufferBarrier.first];

					// we are counting passIndex up, so only the first pass dependent on dependencyPass actually waits on the semaphore
					if (dependencyPass.m_semaphore == VK_NULL_HANDLE)
					{
						dependencyPass.m_semaphore = m_resourceManager.acquireSemaphore();
						pass.m_semaphores.push_back(dependencyPass.m_semaphore);
					}

					// add to end of last pass
					pass.m_endPipelineBarrier.m_srcStageMask |= bufferBarrier.second.m_srcStageMask;
					pass.m_endPipelineBarrier.m_dstStageMask |= bufferBarrier.second.m_dstStageMask;
					pass.m_endPipelineBarrier.m_bufferBarriers.push_back(vkBufferBarrier);
				}

				// back to back dependency? ownership transfer?
				if (bufferBarrier.first == previousPassIndex || interQueueDependency)
				{
					// pipeline barrier
					pass.m_pipelineBarrier.m_srcStageMask |= bufferBarrier.second.m_srcStageMask;
					pass.m_pipelineBarrier.m_dstStageMask |= bufferBarrier.second.m_dstStageMask;
					pass.m_pipelineBarrier.m_bufferBarriers.push_back(vkBufferBarrier);
				}
				else
				{
					// event
					Pass &dependencyPass = m_passes[bufferBarrier.first];

					if (dependencyPass.m_event == VK_NULL_HANDLE)
					{
						dependencyPass.m_event = m_resourceManager.acquireEvent();
					}
					dependencyPass.m_eventStageMask |= bufferBarrier.second.m_srcStageMask;
					pass.m_waitEvents.m_events.push_back(dependencyPass.m_event);
					pass.m_waitEvents.m_srcStageMask |= bufferBarrier.second.m_srcStageMask;
					pass.m_waitEvents.m_dstStageMask |= bufferBarrier.second.m_dstStageMask;
					pass.m_waitEvents.m_bufferBarriers.push_back(vkBufferBarrier);
				}
			}

			for (auto &imageBarrier : pass.m_imageBarriers)
			{
				// transition is handled as part of a renderpass
				if (imageBarrier.second.m_renderPassInternal)
				{
					continue;
				}

				VkImageMemoryBarrier vkImageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				vkImageBarrier.srcAccessMask = imageBarrier.second.m_srcAccessMask;
				vkImageBarrier.dstAccessMask = imageBarrier.second.m_dstAccessMask;
				vkImageBarrier.oldLayout = imageBarrier.second.m_oldLayout;
				vkImageBarrier.newLayout = imageBarrier.second.m_newLayout;
				vkImageBarrier.srcQueueFamilyIndex = imageBarrier.second.m_srcQueueFamilyIndex;
				vkImageBarrier.dstQueueFamilyIndex = imageBarrier.second.m_dstQueueFamilyIndex;
				vkImageBarrier.image = imageBarrier.second.m_image;
				vkImageBarrier.subresourceRange = imageBarrier.second.m_subresourceRange;

				bool interQueueDependency = pass.m_queue != m_passes[imageBarrier.first].m_queue;

				if (interQueueDependency)
				{
					// semaphore
					Pass &dependencyPass = m_passes[imageBarrier.first];

					// we are counting passIndex up, so only the first pass dependent on dependencyPass actually waits on the semaphore
					if (dependencyPass.m_semaphore == VK_NULL_HANDLE)
					{
						dependencyPass.m_semaphore = m_resourceManager.acquireSemaphore();
						pass.m_semaphores.push_back(dependencyPass.m_semaphore);
					}

					// add to end of last pass
					pass.m_endPipelineBarrier.m_srcStageMask |= imageBarrier.second.m_srcStageMask;
					pass.m_endPipelineBarrier.m_dstStageMask |= imageBarrier.second.m_dstStageMask;
					pass.m_endPipelineBarrier.m_imageBarriers.push_back(vkImageBarrier);
				}

				// back to back dependency? ownership transfer?
				if (imageBarrier.first == previousPassIndex || interQueueDependency)
				{
					// pipeline barrier
					pass.m_pipelineBarrier.m_srcStageMask |= imageBarrier.second.m_srcStageMask;
					pass.m_pipelineBarrier.m_dstStageMask |= imageBarrier.second.m_dstStageMask;
					pass.m_pipelineBarrier.m_imageBarriers.push_back(vkImageBarrier);
				}
				else
				{
					// event
					Pass &dependencyPass = m_passes[imageBarrier.first];

					if (dependencyPass.m_event == VK_NULL_HANDLE)
					{
						dependencyPass.m_event = m_resourceManager.acquireEvent();
					}
					dependencyPass.m_eventStageMask |= imageBarrier.second.m_srcStageMask;
					pass.m_waitEvents.m_events.push_back(dependencyPass.m_event);
					pass.m_waitEvents.m_srcStageMask |= imageBarrier.second.m_srcStageMask;
					pass.m_waitEvents.m_dstStageMask |= imageBarrier.second.m_dstStageMask;
					pass.m_waitEvents.m_imageBarriers.push_back(vkImageBarrier);
				}
			}

			for (auto &memoryBarrier : pass.m_memoryBarriers)
			{
				bool interQueueDependency = pass.m_queue != m_passes[memoryBarrier.first].m_queue;

				if (interQueueDependency)
				{
					// semaphore
					Pass &dependencyPass = m_passes[memoryBarrier.first];

					// we are counting passIndex up, so only the first pass dependent on dependencyPass actually waits on the semaphore
					if (dependencyPass.m_semaphore == VK_NULL_HANDLE)
					{
						dependencyPass.m_semaphore = m_resourceManager.acquireSemaphore();
						pass.m_semaphores.push_back(dependencyPass.m_semaphore);
					}
				}
				else
				{
					// back to back dependency?
					if (memoryBarrier.first == previousPassIndex)
					{
						// back to back graphics-graphics dependencies are handled by the renderpass
						if (previousPass.m_queue == QueueType::GRAPHICS && pass.m_passType == PassType::GRAPHICS)
						{
							continue;
						}

						// pipeline barrier
						pass.m_pipelineBarrier.m_srcStageMask |= memoryBarrier.second.m_srcStageMask;
						pass.m_pipelineBarrier.m_dstStageMask |= memoryBarrier.second.m_dstStageMask;
						pass.m_pipelineBarrier.m_memoryBarrier.srcAccessMask = memoryBarrier.second.m_srcAccessMask;
						pass.m_pipelineBarrier.m_memoryBarrier.dstAccessMask = memoryBarrier.second.m_dstAccessMask;
					}
					else
					{
						// event
						Pass &dependencyPass = m_passes[memoryBarrier.first];

						if (dependencyPass.m_event == VK_NULL_HANDLE)
						{
							dependencyPass.m_event = m_resourceManager.acquireEvent();
						}
						dependencyPass.m_eventStageMask |= memoryBarrier.second.m_srcStageMask;
						pass.m_waitEvents.m_events.push_back(dependencyPass.m_event);
						pass.m_waitEvents.m_srcStageMask |= memoryBarrier.second.m_srcStageMask;
						pass.m_waitEvents.m_dstStageMask |= memoryBarrier.second.m_dstStageMask;
						pass.m_waitEvents.m_memoryBarrier.srcAccessMask = memoryBarrier.second.m_srcAccessMask;
						pass.m_waitEvents.m_memoryBarrier.dstAccessMask = memoryBarrier.second.m_dstAccessMask;
					}
				}
			}

			for (auto &executionBarrier : pass.m_executionBarriers)
			{
				bool interQueueDependency = pass.m_queue != m_passes[executionBarrier.first].m_queue;

				if (interQueueDependency)
				{
					// semaphore
					Pass &dependencyPass = m_passes[executionBarrier.first];

					// we are counting passIndex up, so only the first pass dependent on dependencyPass actually waits on the semaphore
					if (dependencyPass.m_semaphore == VK_NULL_HANDLE)
					{
						dependencyPass.m_semaphore = m_resourceManager.acquireSemaphore();
						pass.m_semaphores.push_back(dependencyPass.m_semaphore);
					}
				}
				else
				{
					// back to back dependency?
					if (executionBarrier.first == previousPassIndex)
					{
						// back to back graphics-graphics dependencies are handled by the renderpass
						if (previousPass.m_queue == QueueType::GRAPHICS && pass.m_passType == PassType::GRAPHICS)
						{
							continue;
						}

						// pipeline barrier
						pass.m_pipelineBarrier.m_srcStageMask |= executionBarrier.second.m_srcStageMask;
						pass.m_pipelineBarrier.m_dstStageMask |= executionBarrier.second.m_dstStageMask;
					}
					else
					{
						// event
						Pass &dependencyPass = m_passes[executionBarrier.first];

						if (dependencyPass.m_event == VK_NULL_HANDLE)
						{
							dependencyPass.m_event = m_resourceManager.acquireEvent();
						}
						dependencyPass.m_eventStageMask |= executionBarrier.second.m_srcStageMask;
						pass.m_waitEvents.m_events.push_back(dependencyPass.m_event);
						pass.m_waitEvents.m_srcStageMask |= executionBarrier.second.m_srcStageMask;
						pass.m_waitEvents.m_dstStageMask |= executionBarrier.second.m_dstStageMask;
					}
				}
			}
		}
		previousPassIndex = passIndex;
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

VkImage VEngine::FrameGraph::ResourceRegistry::getImage(ImageHandle handle) const
{
	return m_images[m_virtualImages[(size_t)handle - 1].m_resourceIndex].getImage();
}

VkImageView VEngine::FrameGraph::ResourceRegistry::getImageView(ImageHandle handle) const
{
	return m_imageViews[m_virtualImages[(size_t)handle - 1].m_resourceIndex];
}

VkBuffer VEngine::FrameGraph::ResourceRegistry::getBuffer(BufferHandle handle) const
{
	return m_buffers[m_virtualBuffers[(size_t)handle - 1].m_resourceIndex].getBuffer();
}
