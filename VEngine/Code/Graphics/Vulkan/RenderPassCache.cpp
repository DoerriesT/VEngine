#include "RenderPassCache.h"
#include "VKContext.h"
#include "Utility/Utility.h"

void VEngine::RenderPassCache::getRenderPass(const RenderPassDescription &renderPassDesc, RenderPassCompatibilityDescription &compatDesc, VkRenderPass &renderPass)
{
	// get renderPass from cache or create a new one
	{
		renderPass = m_graphicsPipelines[renderPassDesc];

		// renderPass does not exist yet -> create it
		if (renderPass == VK_NULL_HANDLE)
		{
			VkSubpassDescription subpasses[RenderPassDescription::MAX_SUBPASSES];

			for (size_t i = 0; i < renderPassDesc.m_subpassCount; ++i)
			{
				const auto &subpassDesc = renderPassDesc.m_subpasses[i];
				auto &subpass = subpasses[i];
				subpass.flags = 0;
				subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpass.inputAttachmentCount = subpassDesc.m_inputAttachmentCount;
				subpass.pInputAttachments = subpassDesc.m_inputAttachments;
				subpass.colorAttachmentCount = subpassDesc.m_colorAttachmentCount;
				subpass.pColorAttachments = subpassDesc.m_colorAttachments;
				subpass.pResolveAttachments = nullptr;// subpassDesc.m_resolveAttachments; // TODO
				subpass.pDepthStencilAttachment = subpassDesc.m_depthStencilAttachmentPresent ? &subpassDesc.m_depthStencilAttachment : nullptr;
				subpass.preserveAttachmentCount = subpassDesc.m_preserveAttachmentCount;
				subpass.pPreserveAttachments = subpassDesc.m_preserveAttachments;
			}

			VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = renderPassDesc.m_attachmentCount;
			renderPassInfo.pAttachments = renderPassDesc.m_attachments;
			renderPassInfo.subpassCount = renderPassDesc.m_subpassCount;
			renderPassInfo.pSubpasses = subpasses;
			renderPassInfo.dependencyCount = renderPassDesc.m_dependencyCount;
			renderPassInfo.pDependencies = renderPassDesc.m_dependencies;

			if (vkCreateRenderPass(g_context.m_device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create RenderPass!", EXIT_FAILURE);
			}
		}
	}

	// create compatibility description
	{
		memset(&compatDesc, 0, sizeof(compatDesc));

		compatDesc.m_attachmentCount = renderPassDesc.m_attachmentCount;
		compatDesc.m_subpassCount = renderPassDesc.m_subpassCount;
		compatDesc.m_dependencyCount = renderPassDesc.m_dependencyCount;

		for (size_t i = 0; i < renderPassDesc.m_attachmentCount; ++i)
		{
			compatDesc.m_attachments[i] = { renderPassDesc.m_attachments[i].format, renderPassDesc.m_attachments[i].samples };
		}

		for (size_t i = 0; i < renderPassDesc.m_subpassCount; ++i)
		{
			const auto &subpassDesc = renderPassDesc.m_subpasses[i];
			auto &compatSubpass = compatDesc.m_subpasses[i];
			compatSubpass = {};
			compatSubpass.m_inputAttachmentCount = subpassDesc.m_inputAttachmentCount;
			compatSubpass.m_colorAttachmentCount = subpassDesc.m_colorAttachmentCount;
			compatSubpass.m_depthStencilAttachmentPresent = subpassDesc.m_depthStencilAttachmentPresent;
			compatSubpass.m_preserveAttachmentCount = subpassDesc.m_preserveAttachmentCount;

			for (size_t j = 0; j < subpassDesc.m_inputAttachmentCount; ++j)
			{
				compatSubpass.m_inputAttachments[j] = subpassDesc.m_inputAttachments[j].attachment;
			}

			for (size_t j = 0; j < subpassDesc.m_colorAttachmentCount; ++j)
			{
				compatSubpass.m_colorAttachments[j] = subpassDesc.m_colorAttachments[j].attachment;
				compatSubpass.m_resolveAttachments[j] = subpassDesc.m_resolveAttachments[j].attachment;
			}

			compatSubpass.m_depthStencilAttachment = subpassDesc.m_depthStencilAttachment.attachment;
			memcpy(compatSubpass.m_preserveAttachments, subpassDesc.m_preserveAttachments, sizeof(compatSubpass.m_preserveAttachments));
		}

		memcpy(compatDesc.m_dependencies, renderPassDesc.m_dependencies, sizeof(compatDesc.m_dependencies));

		compatDesc.finalize();
	}
}
