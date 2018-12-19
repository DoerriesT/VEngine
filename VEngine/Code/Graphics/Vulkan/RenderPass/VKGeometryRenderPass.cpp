#include "VKGeometryRenderPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/Pipeline/VKGeometryPipeline.h"
#include "Graphics/Vulkan/Pipeline/VKGeometryAlphaMaskPipeline.h"
#include "Utility/Utility.h"

VEngine::VKGeometryRenderPass::VKGeometryRenderPass(VKRenderResources *renderResources)
	:m_geometryPipeline(),
	m_geometryAlphaMaskPipeline()
{
	VkAttachmentDescription attachmentDescriptions[5] = {};

	// depth
	attachmentDescriptions[0].format = renderResources->m_depthAttachment.m_format;
	attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// albedo
	attachmentDescriptions[1].format = renderResources->m_albedoAttachment.m_format;
	attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// normal
	attachmentDescriptions[2].format = renderResources->m_normalAttachment.m_format;
	attachmentDescriptions[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// material
	attachmentDescriptions[3].format = renderResources->m_materialAttachment.m_format;
	attachmentDescriptions[3].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[3].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// velocity
	attachmentDescriptions[4].format = renderResources->m_velocityAttachment.m_format;
	attachmentDescriptions[4].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[4].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[4].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;


	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 0;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference geometryAttachmentRefs[] =
	{
		{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
	};

	VkSubpassDescription geometrySubpass = {};
	geometrySubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	geometrySubpass.colorAttachmentCount = sizeof(geometryAttachmentRefs) / sizeof(geometryAttachmentRefs[0]);
	geometrySubpass.pColorAttachments = geometryAttachmentRefs;
	geometrySubpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDescription geometryAlphaMaskSubpass = {};
	geometryAlphaMaskSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	geometryAlphaMaskSubpass.colorAttachmentCount = sizeof(geometryAttachmentRefs) / sizeof(geometryAttachmentRefs[0]);
	geometryAlphaMaskSubpass.pColorAttachments = geometryAttachmentRefs;
	geometryAlphaMaskSubpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependencies[2];

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

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkSubpassDescription subpasses[] = { geometrySubpass, geometryAlphaMaskSubpass };

	VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	renderPassInfo.attachmentCount = static_cast<uint32_t>(sizeof(attachmentDescriptions) / sizeof(attachmentDescriptions[0]));
	renderPassInfo.pAttachments = attachmentDescriptions;
	renderPassInfo.subpassCount = static_cast<uint32_t>(sizeof(subpasses) / sizeof(subpasses[0]));
	renderPassInfo.pSubpasses = subpasses;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(sizeof(dependencies) / sizeof(dependencies[0]));
	renderPassInfo.pDependencies = dependencies;

	if (vkCreateRenderPass(g_context.m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to create render pass!", -1);
	}
}

VEngine::VKGeometryRenderPass::~VKGeometryRenderPass()
{
	vkDestroyRenderPass(g_context.m_device, m_renderPass, nullptr);
}

VkRenderPass VEngine::VKGeometryRenderPass::get()
{
	return m_renderPass;
}

void VEngine::VKGeometryRenderPass::record(VKRenderResources *renderResources, const DrawLists &drawLists, uint32_t width, uint32_t height)
{
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(renderResources->m_geometryFillCommandBuffer, &beginInfo);
	{
		VkClearValue clearValues[2] = {};
		clearValues[0].depthStencil = { 1.0f, 0 };
		clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };


		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_renderPass;
		renderPassInfo.framebuffer = renderResources->m_geometryFillFramebuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { width, height };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(sizeof(clearValues) / sizeof(clearValues[0]));
		renderPassInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(renderResources->m_geometryFillCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			// geometry pass
			m_geometryPipeline->recordCommandBuffer(m_renderPass, renderResources, drawLists);

			// geometry alpha mask pass
			vkCmdNextSubpass(renderResources->m_geometryFillCommandBuffer, VK_SUBPASS_CONTENTS_INLINE);
			m_geometryAlphaMaskPipeline->recordCommandBuffer(m_renderPass, renderResources, drawLists);
		}
		vkCmdEndRenderPass(renderResources->m_geometryFillCommandBuffer);
	}
	vkEndCommandBuffer(renderResources->m_geometryFillCommandBuffer);
}

void VEngine::VKGeometryRenderPass::submit(VKRenderResources *renderResources)
{
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &renderResources->m_geometryFillCommandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

	if (vkQueueSubmit(g_context.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to submit draw command buffer!", -1);
	}
}

void VEngine::VKGeometryRenderPass::setPipelines(VKGeometryPipeline *geometryPipeline, VKGeometryAlphaMaskPipeline *geometryAlphaMaskPipeline)
{
	m_geometryPipeline = geometryPipeline;
	m_geometryAlphaMaskPipeline = geometryAlphaMaskPipeline;
}
