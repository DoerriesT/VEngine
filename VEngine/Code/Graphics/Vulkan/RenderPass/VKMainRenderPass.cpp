#include "VKMainRenderPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Utility/Utility.h"

VEngine::VKMainRenderPass::VKMainRenderPass(VKRenderResources *renderResources)
{
	VkAttachmentDescription attachmentDescriptions[6] = {};

	// depth
	attachmentDescriptions[0].format = renderResources->m_depthAttachment.m_format;
	attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// albedo
	attachmentDescriptions[1].format = renderResources->m_albedoAttachment.m_format;
	attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// normal
	attachmentDescriptions[2].format = renderResources->m_normalAttachment.m_format;
	attachmentDescriptions[2].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// material
	attachmentDescriptions[3].format = renderResources->m_materialAttachment.m_format;
	attachmentDescriptions[3].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[3].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
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

	// light
	attachmentDescriptions[5].format = renderResources->m_lightAttachment.m_format;
	attachmentDescriptions[5].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescriptions[5].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[5].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescriptions[5].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[5].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[5].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[5].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;


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

	VkAttachmentReference lightingInputRefs[] =
	{
		{ 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
		{ 3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
	};

	VkAttachmentReference lightingAttachmentRefs[] =
	{
		{ 5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
	};

	VkAttachmentReference forwardAttachmentRefs[] =
	{
		{ 5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
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

	VkSubpassDescription lightingSubpass = {};
	lightingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	lightingSubpass.inputAttachmentCount = sizeof(lightingInputRefs) / sizeof(lightingInputRefs[0]);
	lightingSubpass.pInputAttachments = lightingInputRefs;
	lightingSubpass.colorAttachmentCount = sizeof(lightingAttachmentRefs) / sizeof(lightingAttachmentRefs[0]);
	lightingSubpass.pColorAttachments = lightingAttachmentRefs;
	lightingSubpass.pDepthStencilAttachment = nullptr;

	VkSubpassDescription forwardSubpass = {};
	forwardSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	forwardSubpass.colorAttachmentCount = sizeof(forwardAttachmentRefs) / sizeof(forwardAttachmentRefs[0]);;
	forwardSubpass.pColorAttachments = forwardAttachmentRefs;
	forwardSubpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependencies[4];

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

	dependencies[2].srcSubpass = 1;
	dependencies[2].dstSubpass = 2;
	dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[2].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[3].srcSubpass = 2;
	dependencies[3].dstSubpass = 3;
	dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[3].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
		| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[3].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
		| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[3].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkSubpassDescription subpasses[] = { geometrySubpass, geometryAlphaMaskSubpass, lightingSubpass, forwardSubpass };

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

VEngine::VKMainRenderPass::~VKMainRenderPass()
{
	vkDestroyRenderPass(g_context.m_device, m_renderPass, nullptr);
}