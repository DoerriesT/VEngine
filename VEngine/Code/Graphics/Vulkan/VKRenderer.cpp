#include "VKRenderer.h"
#include "VKSwapChain.h"
#include "VKRenderResources.h"
#include "Pipeline/VKForwardPipeline.h"
#include "Utility/Utility.h"
#include "VKUtility.h"
#include "Graphics/Model.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include "Graphics/Camera/Camera.h"
#include "Graphics/RenderParams.h"

extern VEngine::VKContext g_context;

VEngine::VKRenderer::VKRenderer()
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
	m_swapChain.reset(new VKSwapChain());
	m_forwardPipeline.reset(new VKForwardPipeline());

	m_renderResources->init(width, height);
	m_swapChain->init(width, height);
	m_forwardPipeline->init(width, height, m_renderResources.get());
	static std::shared_ptr<VEngine::Model> model = std::shared_ptr<VEngine::Model>(new Model("Resources/Models/ice.obj"));
	m_forwardPipeline->recordCommandBuffer({ model });
}

void VEngine::VKRenderer::update(const RenderParams &renderParams)
{
	UBO ubo = {};

	ubo.model = glm::mat4();
	ubo.view = renderParams.m_viewMatrix;
	ubo.projection = renderParams.m_projectionMatrix;

	auto buffer = m_forwardPipeline->getUniformBuffer();
	void* data;
	vkMapMemory(g_context.m_device, buffer.m_memory, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(g_context.m_device, buffer.m_memory);
}

void VEngine::VKRenderer::render()
{
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(g_context.m_device, m_swapChain->get(), std::numeric_limits<uint64_t>::max(), g_context.m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_swapChain->recreate(m_width, m_height);
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		Utility::fatalExit("Failed to acquire swap chain image!", -1);
	}

	VkSemaphore waitSemaphores[] = { g_context.m_imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	VkCommandBuffer commandBuffer = m_forwardPipeline->getCommandBuffer();
	submitInfo.pCommandBuffers = &commandBuffer;

	VkSemaphore signalSemaphores[] = { g_context.m_renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(g_context.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to submit draw command buffer!", -1);
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

		VkCommandBuffer blitCommandBuffer = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
		{
			VKUtility::setImageLayout(blitCommandBuffer, m_swapChain->getImage(imageIndex), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

			vkCmdBlitImage(blitCommandBuffer, m_renderResources->m_colorAttachment.m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_swapChain->getImage(imageIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlitRegion, VK_FILTER_NEAREST);

			VKUtility::setImageLayout(blitCommandBuffer, m_swapChain->getImage(imageIndex), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
		}
		VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, blitCommandBuffer);

	}

	VkSwapchainKHR swapChains[] = { m_swapChain->get() };

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(g_context.m_presentQueue, &presentInfo);

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

void VEngine::VKRenderer::reserveMeshBuffer(uint64_t size)
{
	m_renderResources->reserveMeshBuffer(size);
}

void VEngine::VKRenderer::uploadMeshData(const unsigned char * vertices, uint64_t vertexSize, const unsigned char * indices, uint64_t indexSize)
{
	m_renderResources->uploadMeshData(vertices, vertexSize, indices, indexSize);
}
