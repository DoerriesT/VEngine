#include "VKRenderer.h"
#include "VKSwapChain.h"
#include "VKRenderResources.h"
#include "Pipeline/VKDepthPrepassPipeline.h"
#include "Pipeline/VKDepthPrepassAlphaMaskPipeline.h"
#include "Pipeline/VKForwardPipeline.h"
#include "Utility/Utility.h"
#include "VKUtility.h"
#include "Graphics/RenderParams.h"
#include "Graphics/DrawItem.h"
#include "VKTextureLoader.h"

extern VEngine::VKContext g_context;

VEngine::VKRenderer::VKRenderer()
	:m_width(),
	m_height(),
	m_swapChainImageIndex(),
	m_mainRenderPass()
{
}

VEngine::VKRenderer::~VKRenderer()
{
}

void VEngine::VKRenderer::init(unsigned int width, unsigned int height)
{
	m_width = width;
	m_height = height;
	m_renderResources.reset(new VKRenderResources());
	m_textureLoader.reset(new VKTextureLoader());
	m_swapChain.reset(new VKSwapChain());
	m_depthPrepassPipeline.reset(new VKDepthPrepassPipeline());
	m_depthPrepassAlphaMaskPipeline.reset(new VKDepthPrepassAlphaMaskPipeline());
	m_forwardPipeline.reset(new VKForwardPipeline());

	m_renderResources->init(width, height);
	m_swapChain->init(width, height);

	// create main renderpass
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = m_renderResources->m_colorAttachment.m_format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = m_renderResources->m_depthAttachment.m_format;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription depthSubpass = {};
		depthSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		depthSubpass.colorAttachmentCount = 0;
		depthSubpass.pColorAttachments = nullptr;
		depthSubpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDescription depthMaskedSubpass = {};
		depthMaskedSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		depthMaskedSubpass.colorAttachmentCount = 0;
		depthMaskedSubpass.pColorAttachments = nullptr;
		depthMaskedSubpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDescription forwardSubpass = {};
		forwardSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		forwardSubpass.colorAttachmentCount = 1;
		forwardSubpass.pColorAttachments = &colorAttachmentRef;
		forwardSubpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency colorDependency = {};
		colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		colorDependency.dstSubpass = 2;
		colorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorDependency.srcAccessMask = 0;
		colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkSubpassDependency depthDependency = {};
		depthDependency.srcSubpass = 0;
		depthDependency.dstSubpass = 2;
		depthDependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

		VkSubpassDependency depthDepthDependency = {};
		depthDepthDependency.srcSubpass = 0;
		depthDepthDependency.dstSubpass = 1;
		depthDepthDependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthDepthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		depthDepthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		depthDepthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkSubpassDependency depthMaskedDependency = {};
		depthMaskedDependency.srcSubpass = 1;
		depthMaskedDependency.dstSubpass = 2;
		depthMaskedDependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depthMaskedDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		depthMaskedDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		depthMaskedDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

		VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

		VkSubpassDescription subpasses[] = { depthSubpass, depthMaskedSubpass ,forwardSubpass };
		VkSubpassDependency dependencies[] = { colorDependency , depthDepthDependency, depthDependency, depthMaskedDependency };

		VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
		renderPassInfo.attachmentCount = static_cast<uint32_t>(sizeof(attachments) / sizeof(attachments[0]));
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = static_cast<uint32_t>(sizeof(subpasses) / sizeof(subpasses[0]));
		renderPassInfo.pSubpasses = subpasses;
		renderPassInfo.dependencyCount = static_cast<uint32_t>(sizeof(dependencies) / sizeof(dependencies[0]));
		renderPassInfo.pDependencies = dependencies;

		if (vkCreateRenderPass(g_context.m_device, &renderPassInfo, nullptr, &m_mainRenderPass) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create render pass!", -1);
		}
	}

	m_renderResources->createFramebuffer(width, height, m_mainRenderPass);
	m_renderResources->createUniformBuffer(sizeof(RenderParams), sizeof(PerDrawData));
	m_renderResources->createCommandBuffers();
	m_renderResources->createDummyTexture();
	m_renderResources->createDescriptors();

	m_depthPrepassPipeline->init(width, height, m_mainRenderPass, m_renderResources.get());
	m_depthPrepassAlphaMaskPipeline->init(width, height, m_mainRenderPass, m_renderResources.get());
	m_forwardPipeline->init(width, height, m_mainRenderPass, m_renderResources.get());

}

void VEngine::VKRenderer::update(const RenderParams &renderParams, const DrawLists &drawLists)
{
	if (drawLists.m_opaqueItems.size() + drawLists.m_maskedItems.size() + drawLists.m_blendedItems.size() > MAX_UNIFORM_BUFFER_INSTANCE_COUNT)
	{
		Utility::fatalExit("Exceeded max DrawItem count!", -1);
	}

	void *data;
	vkMapMemory(g_context.m_device, m_renderResources->m_mainUniformBuffer.m_memory, 0, m_renderResources->m_mainUniformBuffer.m_size, 0, &data);

	// per frame data
	{
		RenderParams *perFrameData = (RenderParams *)data;
		*perFrameData = renderParams;
	}

	// per draw data
	{
		unsigned char *perDrawData = ((unsigned char *)data) + m_renderResources->m_perFrameDataSize;

		for (const DrawItem &item : drawLists.m_opaqueItems)
		{
			memcpy(perDrawData, &item.m_perDrawData, sizeof(item.m_perDrawData));
			perDrawData += m_renderResources->m_perDrawDataSize;
		}

		for (const DrawItem &item : drawLists.m_maskedItems)
		{
			memcpy(perDrawData, &item.m_perDrawData, sizeof(item.m_perDrawData));
			perDrawData += m_renderResources->m_perDrawDataSize;
		}

		for (const DrawItem &item : drawLists.m_blendedItems)
		{
			memcpy(perDrawData, &item.m_perDrawData, sizeof(item.m_perDrawData));
			perDrawData += m_renderResources->m_perDrawDataSize;
		}
	}

	vkUnmapMemory(g_context.m_device, m_renderResources->m_mainUniformBuffer.m_memory);


	VkResult result = vkAcquireNextImageKHR(g_context.m_device, m_swapChain->get(), std::numeric_limits<uint64_t>::max(), g_context.m_imageAvailableSemaphore, VK_NULL_HANDLE, &m_swapChainImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_swapChain->recreate(m_width, m_height);
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		Utility::fatalExit("Failed to acquire swap chain image!", -1);
	}

	// record commandbuffers
	{
		m_depthPrepassPipeline->recordCommandBuffer(m_mainRenderPass, m_renderResources.get(), drawLists);
		m_depthPrepassAlphaMaskPipeline->recordCommandBuffer(m_mainRenderPass, m_renderResources.get(), drawLists);
		m_forwardPipeline->recordCommandBuffer(m_mainRenderPass, m_renderResources.get(), drawLists);

		// main
		{
			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(m_renderResources->m_mainCommandBuffer, &beginInfo);
			{
				VkClearValue clearValues[2] = {};
				clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
				clearValues[1].depthStencil = { 1.0f, 0 };

				VkRenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.renderPass = m_mainRenderPass;
				renderPassInfo.framebuffer = m_renderResources->m_mainFramebuffer;
				renderPassInfo.renderArea.offset = { 0, 0 };
				renderPassInfo.renderArea.extent = { m_width, m_height };
				renderPassInfo.clearValueCount = static_cast<uint32_t>(sizeof(clearValues) / sizeof(clearValues[0]));
				renderPassInfo.pClearValues = clearValues;

				vkCmdBeginRenderPass(m_renderResources->m_mainCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
				{
					// depth prepass
					vkCmdExecuteCommands(m_renderResources->m_mainCommandBuffer, 1, &m_renderResources->m_depthPrepassCommandBuffer);

					// depth prepass alpha mask
					vkCmdNextSubpass(m_renderResources->m_mainCommandBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
					vkCmdExecuteCommands(m_renderResources->m_mainCommandBuffer, 1, &m_renderResources->m_depthPrepassAlphaMaskCommandBuffer);

					// forward pass
					vkCmdNextSubpass(m_renderResources->m_mainCommandBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
					vkCmdExecuteCommands(m_renderResources->m_mainCommandBuffer, 1, &m_renderResources->m_forwardCommandBuffer);
				}
				vkCmdEndRenderPass(m_renderResources->m_mainCommandBuffer);

				VkOffset3D blitSize;
				blitSize.x = m_width;
				blitSize.y = m_height;
				blitSize.z = 1;

				VkImageBlit imageBlitRegion = {};
				imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageBlitRegion.srcSubresource.layerCount = 1;
				imageBlitRegion.srcOffsets[1] = blitSize;
				imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageBlitRegion.dstSubresource.layerCount = 1;
				imageBlitRegion.dstOffsets[1] = blitSize;

				VkImageSubresourceRange subresourceRange = {};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.baseMipLevel = 0;
				subresourceRange.levelCount = 1;
				subresourceRange.baseArrayLayer = 0;
				subresourceRange.layerCount = 1;

				VKUtility::setImageLayout(
					m_renderResources->m_mainCommandBuffer,
					m_swapChain->getImage(m_swapChainImageIndex),
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					subresourceRange,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT);

				vkCmdBlitImage(
					m_renderResources->m_mainCommandBuffer,
					m_renderResources->m_colorAttachment.m_image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					m_swapChain->getImage(m_swapChainImageIndex),
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&imageBlitRegion,
					VK_FILTER_NEAREST);

				VKUtility::setImageLayout(
					m_renderResources->m_mainCommandBuffer,
					m_swapChain->getImage(m_swapChainImageIndex),
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					subresourceRange,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
			}
			vkEndCommandBuffer(m_renderResources->m_mainCommandBuffer);
		}
	}
}

void VEngine::VKRenderer::render()
{
	VkSemaphore waitSemaphores[] = { g_context.m_imageAvailableSemaphore };
	VkSemaphore signalSemaphores[] = { g_context.m_renderFinishedSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_renderResources->m_mainCommandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(g_context.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to submit draw command buffer!", -1);
	}

	VkSwapchainKHR swapChains[] = { m_swapChain->get() };

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_swapChainImageIndex;

	VkResult result = vkQueuePresentKHR(g_context.m_presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		m_swapChain->recreate(m_width, m_height);
	}
	else if (result != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to present swap chain image!", -1);
	}

	vkQueueWaitIdle(g_context.m_presentQueue);
}

void VEngine::VKRenderer::reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize)
{
	m_renderResources->reserveMeshBuffers(vertexSize, indexSize);
}

void VEngine::VKRenderer::uploadMeshData(const unsigned char * vertices, uint64_t vertexSize, const unsigned char * indices, uint64_t indexSize)
{
	m_renderResources->uploadMeshData(vertices, vertexSize, indices, indexSize);
}

uint32_t VEngine::VKRenderer::loadTexture(const char *filepath)
{
	return m_textureLoader->load(filepath);
}

void VEngine::VKRenderer::freeTexture(uint32_t id)
{
	m_textureLoader->free(id);
}

void VEngine::VKRenderer::updateTextureData()
{
	m_renderResources->updateTextureArray(m_textureLoader->getTextures());
}
