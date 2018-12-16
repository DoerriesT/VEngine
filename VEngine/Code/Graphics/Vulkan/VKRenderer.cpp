#include "VKRenderer.h"
#include "VKSwapChain.h"
#include "VKRenderResources.h"
#include "RenderPass/VKShadowRenderPass.h"
#include "RenderPass/VKMainRenderPass.h"
#include "Pipeline/VKShadowPipeline.h"
#include "Pipeline/VKGeometryPipeline.h"
#include "Pipeline/VKGeometryAlphaMaskPipeline.h"
#include "Pipeline/VKLightingPipeline.h"
#include "Pipeline/VKForwardPipeline.h"
#include "Utility/Utility.h"
#include "VKUtility.h"
#include "Graphics/RenderParams.h"
#include "Graphics/DrawItem.h"
#include "VKTextureLoader.h"
#include "GlobalVar.h"

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
	m_shadowPipeline.reset(new VKShadowPipeline());
	m_geometryPipeline.reset(new VKGeometryPipeline());
	m_geometryAlphaMaskPipeline.reset(new VKGeometryAlphaMaskPipeline()),
		m_lightingPipeline.reset(new VKLightingPipeline());
	m_forwardPipeline.reset(new VKForwardPipeline());

	m_renderResources->init(width, height);
	m_swapChain->init(width, height);

	m_shadowRenderPass.reset(new VKShadowRenderPass(m_renderResources.get()));
	m_mainRenderPass.reset(new VKMainRenderPass(m_renderResources.get()));

	m_renderResources->createFramebuffer(width, height, m_mainRenderPass->m_renderPass, m_shadowRenderPass->m_renderPass);
	m_renderResources->createUniformBuffer(sizeof(RenderParams), sizeof(PerDrawData));
	m_renderResources->createCommandBuffers();
	m_renderResources->createDummyTexture();
	m_renderResources->createDescriptors();

	m_shadowPipeline->init(m_shadowRenderPass->m_renderPass, m_renderResources.get());
	m_geometryPipeline->init(width, height, m_mainRenderPass->m_renderPass, m_renderResources.get());
	m_geometryAlphaMaskPipeline->init(width, height, m_mainRenderPass->m_renderPass, m_renderResources.get());
	m_lightingPipeline->init(width, height, m_mainRenderPass->m_renderPass, m_renderResources.get());
	m_forwardPipeline->init(width, height, m_mainRenderPass->m_renderPass, m_renderResources.get());

}

void VEngine::VKRenderer::update(const RenderParams &renderParams, const DrawLists &drawLists, const LightData &lightData)
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
		m_shadowPipeline->recordCommandBuffer(m_shadowRenderPass->m_renderPass, m_renderResources.get(), drawLists, lightData);
		m_geometryPipeline->recordCommandBuffer(m_mainRenderPass->m_renderPass, m_renderResources.get(), drawLists);
		m_geometryAlphaMaskPipeline->recordCommandBuffer(m_mainRenderPass->m_renderPass, m_renderResources.get(), drawLists);
		m_lightingPipeline->recordCommandBuffer(m_mainRenderPass->m_renderPass, m_renderResources.get());
		m_forwardPipeline->recordCommandBuffer(m_mainRenderPass->m_renderPass, m_renderResources.get(), drawLists);

		// main
		{
			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(m_renderResources->m_mainCommandBuffer, &beginInfo);
			{
				// shadow render pass
				{
					VkRenderPassBeginInfo renderPassInfo = {};
					renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
					renderPassInfo.renderPass = m_shadowRenderPass->m_renderPass;
					renderPassInfo.framebuffer = m_renderResources->m_shadowFramebuffer;
					renderPassInfo.renderArea.offset = { 0, 0 };
					renderPassInfo.renderArea.extent = { g_shadowAtlasSize, g_shadowAtlasSize };

					vkCmdBeginRenderPass(m_renderResources->m_mainCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
					vkCmdExecuteCommands(m_renderResources->m_mainCommandBuffer, 1, &m_renderResources->m_shadowsCommandBuffer);
					vkCmdEndRenderPass(m_renderResources->m_mainCommandBuffer);
				}

				// main render pass
				{
					VkClearValue clearValues[2] = {};
					clearValues[0].depthStencil = { 1.0f, 0 };
					clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };


					VkRenderPassBeginInfo renderPassInfo = {};
					renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
					renderPassInfo.renderPass = m_mainRenderPass->m_renderPass;
					renderPassInfo.framebuffer = m_renderResources->m_mainFramebuffer;
					renderPassInfo.renderArea.offset = { 0, 0 };
					renderPassInfo.renderArea.extent = { m_width, m_height };
					renderPassInfo.clearValueCount = static_cast<uint32_t>(sizeof(clearValues) / sizeof(clearValues[0]));
					renderPassInfo.pClearValues = clearValues;

					vkCmdBeginRenderPass(m_renderResources->m_mainCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
					{
						// geometry pass
						vkCmdExecuteCommands(m_renderResources->m_mainCommandBuffer, 1, &m_renderResources->m_geometryCommandBuffer);

						// geometry alpha mask pass
						vkCmdNextSubpass(m_renderResources->m_mainCommandBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
						vkCmdExecuteCommands(m_renderResources->m_mainCommandBuffer, 1, &m_renderResources->m_geometryAlphaMaskCommandBuffer);

						// lighting pass
						vkCmdNextSubpass(m_renderResources->m_mainCommandBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
						vkCmdExecuteCommands(m_renderResources->m_mainCommandBuffer, 1, &m_renderResources->m_lightingCommandBuffer);

						// forward pass
						vkCmdNextSubpass(m_renderResources->m_mainCommandBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
						vkCmdExecuteCommands(m_renderResources->m_mainCommandBuffer, 1, &m_renderResources->m_forwardCommandBuffer);
					}
					vkCmdEndRenderPass(m_renderResources->m_mainCommandBuffer);
				}

				// blit
				{
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
						m_renderResources->m_lightAttachment.m_image,
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
