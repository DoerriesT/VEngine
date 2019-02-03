#include "VKForwardRenderPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Utility/Utility.h"
#include "Graphics/Vulkan/Pipeline/VKForwardPipeline.h"

VEngine::VKForwardRenderPass::VKForwardRenderPass(VKRenderResources *renderResources)
	:m_forwardPipeline()
{
	VkAttachmentDescription attachmentDescriptions[3] = {};

	// depth
	attachmentDescriptions[0].format = renderResources->m_depthAttachment.getFormat();
	attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// velocity
	attachmentDescriptions[1].format = renderResources->m_velocityAttachment.getFormat();
	attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// light
	attachmentDescriptions[2].format = renderResources->m_lightAttachment.getFormat();
	attachmentDescriptions[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachmentDescriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[2].initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	attachmentDescriptions[2].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;


	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 0;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference forwardAttachmentRefs[] =
	{
		{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
	};

	VkSubpassDescription forwardSubpass = {};
	forwardSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	forwardSubpass.colorAttachmentCount = sizeof(forwardAttachmentRefs) / sizeof(forwardAttachmentRefs[0]);;
	forwardSubpass.pColorAttachments = forwardAttachmentRefs;
	forwardSubpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependencies[1];

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	renderPassInfo.attachmentCount = static_cast<uint32_t>(sizeof(attachmentDescriptions) / sizeof(attachmentDescriptions[0]));
	renderPassInfo.pAttachments = attachmentDescriptions;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &forwardSubpass;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(sizeof(dependencies) / sizeof(dependencies[0]));
	renderPassInfo.pDependencies = dependencies;

	if (vkCreateRenderPass(g_context.m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create render pass!", -1);
	}
}

VEngine::VKForwardRenderPass::~VKForwardRenderPass()
{
	vkDestroyRenderPass(g_context.m_device, m_renderPass, nullptr);
}

VkRenderPass VEngine::VKForwardRenderPass::get()
{
	return m_renderPass;
}

void VEngine::VKForwardRenderPass::record(VKRenderResources *renderResources, const DrawLists &drawLists, VkImage swapChainImage, uint32_t width, uint32_t height)
{
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(renderResources->m_forwardCommandBuffer, &beginInfo);
	{
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_renderPass;
		renderPassInfo.framebuffer = renderResources->m_forwardFramebuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { width, height };

		vkCmdBeginRenderPass(renderResources->m_forwardCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			m_forwardPipeline->recordCommandBuffer(m_renderPass, renderResources, drawLists);
		}
		vkCmdEndRenderPass(renderResources->m_forwardCommandBuffer);

		VkOffset3D blitSize;
		blitSize.x = width;
		blitSize.y = height;
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
			renderResources->m_forwardCommandBuffer,
			swapChainImage,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

		vkCmdBlitImage(
			renderResources->m_forwardCommandBuffer,
			renderResources->m_lightAttachment.getImage(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			swapChainImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageBlitRegion,
			VK_FILTER_NEAREST);

		VKUtility::setImageLayout(
			renderResources->m_forwardCommandBuffer,
			swapChainImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			subresourceRange,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	}
	vkEndCommandBuffer(renderResources->m_forwardCommandBuffer);
}

void VEngine::VKForwardRenderPass::submit(VKRenderResources *renderResources)
{
	VkSemaphore waitSemaphores[] = { g_context.m_imageAvailableSemaphore };
	VkSemaphore signalSemaphores[] = { g_context.m_renderFinishedSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &renderResources->m_forwardCommandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(g_context.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to submit draw command buffer!", -1);
	}
}

void VEngine::VKForwardRenderPass::setPipelines(VKForwardPipeline *forwardPipeline)
{
	m_forwardPipeline = forwardPipeline;
}
