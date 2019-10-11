#include "VKGeometryPass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Mesh.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/Vulkan/RenderPassCache.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/DeferredObjectDeleter.h"
#include "Utility/Utility.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/geometry_bindings.h"
}

void VEngine::VKGeometryPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::READ_INDIRECT_BUFFER},
		{ResourceViewHandle(data.m_depthImageHandle), ResourceState::WRITE_DEPTH_STENCIL},
		{ResourceViewHandle(data.m_uvImageHandle), ResourceState::WRITE_ATTACHMENT},
		{ResourceViewHandle(data.m_ddxyLengthImageHandle), ResourceState::WRITE_ATTACHMENT},
		{ResourceViewHandle(data.m_ddxyRotMaterialIdImageHandle), ResourceState::WRITE_ATTACHMENT},
		{ResourceViewHandle(data.m_tangentSpaceImageHandle), ResourceState::WRITE_ATTACHMENT},
		{ResourceViewHandle(data.m_drawCountBufferHandle), ResourceState::READ_INDIRECT_BUFFER},
	};

	graph.addPass(data.m_alphaMasked ? "GBuffer Fill Alpha" : "GBuffer Fill", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]) - (data.m_alphaMasked ? 1 : 0), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// begin renderpass
		VkRenderPass renderPass;
		RenderPassCompatibilityDescription renderPassCompatDesc;
		{
			RenderPassDescription renderPassDesc{};
			renderPassDesc.m_attachmentCount = 5;
			renderPassDesc.m_subpassCount = 1;
			renderPassDesc.m_attachments[0] = registry.getAttachmentDescription(data.m_depthImageHandle, ResourceState::WRITE_DEPTH_STENCIL, true);
			renderPassDesc.m_attachments[1] = registry.getAttachmentDescription(data.m_uvImageHandle, ResourceState::WRITE_ATTACHMENT);
			renderPassDesc.m_attachments[2] = registry.getAttachmentDescription(data.m_ddxyLengthImageHandle, ResourceState::WRITE_ATTACHMENT);
			renderPassDesc.m_attachments[3] = registry.getAttachmentDescription(data.m_ddxyRotMaterialIdImageHandle, ResourceState::WRITE_ATTACHMENT);
			renderPassDesc.m_attachments[4] = registry.getAttachmentDescription(data.m_tangentSpaceImageHandle, ResourceState::WRITE_ATTACHMENT);
			renderPassDesc.m_subpasses[0] = { 0, 4, true, 0 };
			renderPassDesc.m_subpasses[0].m_colorAttachments[0] = { 1, renderPassDesc.m_attachments[1].initialLayout };
			renderPassDesc.m_subpasses[0].m_colorAttachments[1] = { 2, renderPassDesc.m_attachments[2].initialLayout };
			renderPassDesc.m_subpasses[0].m_colorAttachments[2] = { 3, renderPassDesc.m_attachments[3].initialLayout };
			renderPassDesc.m_subpasses[0].m_colorAttachments[3] = { 4, renderPassDesc.m_attachments[4].initialLayout };
			renderPassDesc.m_subpasses[0].m_depthStencilAttachment = { 0, renderPassDesc.m_attachments[0].initialLayout };

			data.m_passRecordContext->m_renderPassCache->getRenderPass(renderPassDesc, renderPassCompatDesc, renderPass);

			VkFramebuffer framebuffer;
			VkImageView attachmentViews[] = 
			{
				registry.getImageView(data.m_depthImageHandle),
				registry.getImageView(data.m_uvImageHandle),
				registry.getImageView(data.m_ddxyLengthImageHandle),
				registry.getImageView(data.m_ddxyRotMaterialIdImageHandle),
				registry.getImageView(data.m_tangentSpaceImageHandle),
			};

			VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferCreateInfo.renderPass = renderPass;
			framebufferCreateInfo.attachmentCount = renderPassDesc.m_attachmentCount;
			framebufferCreateInfo.pAttachments = attachmentViews;
			framebufferCreateInfo.width = width;
			framebufferCreateInfo.height = height;
			framebufferCreateInfo.layers = 1;

			if (vkCreateFramebuffer(g_context.m_device, &framebufferCreateInfo, nullptr, &framebuffer) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create framebuffer!", EXIT_FAILURE);
			}

			data.m_passRecordContext->m_deferredObjectDeleter->add(framebuffer);

			VkClearValue clearValues[5];
			clearValues[0].depthStencil = { 0.0f, 0 };

			VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = framebuffer;
			renderPassBeginInfo.renderArea = { {}, {width, height} };
			renderPassBeginInfo.clearValueCount = renderPassDesc.m_attachmentCount;
			renderPassBeginInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		}

		// create pipeline description
		VKGraphicsPipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_vertexShaderStage.m_path, "Resources/Shaders/geometry_vert.spv");
			strcpy_s(pipelineDesc.m_fragmentShaderStage.m_path, data.m_alphaMasked ? "Resources/Shaders/geometry_alpha_mask_frag.spv" : "Resources/Shaders/geometry_frag.spv");

			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 0;
			//pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[0] = { 0, sizeof(VertexPosition), VK_VERTEX_INPUT_RATE_VERTEX };
			//pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[1] = { 1, sizeof(VertexNormal), VK_VERTEX_INPUT_RATE_VERTEX };
			//pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[2] = { 2, sizeof(VertexTexCoord), VK_VERTEX_INPUT_RATE_VERTEX };

			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 0;
			//pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
			//pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[1] = { 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 };
			//pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[2] = { 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 };

			pipelineDesc.m_inputAssemblyState.m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			pipelineDesc.m_inputAssemblyState.m_primitiveRestartEnable = false;

			pipelineDesc.m_viewportState.m_viewportCount = 1;
			pipelineDesc.m_viewportState.m_viewports[0] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
			pipelineDesc.m_viewportState.m_scissorCount = 1;
			pipelineDesc.m_viewportState.m_scissors[0] = { {0, 0}, {1, 1} };

			pipelineDesc.m_rasterizationState.m_depthClampEnable = false;
			pipelineDesc.m_rasterizationState.m_rasterizerDiscardEnable = false;
			pipelineDesc.m_rasterizationState.m_polygonMode = VK_POLYGON_MODE_FILL;
			pipelineDesc.m_rasterizationState.m_cullMode = data.m_alphaMasked ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
			pipelineDesc.m_rasterizationState.m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			pipelineDesc.m_rasterizationState.m_depthBiasEnable = false;
			pipelineDesc.m_rasterizationState.m_lineWidth = 1.0f;

			pipelineDesc.m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			pipelineDesc.m_multiSampleState.m_sampleShadingEnable = false;
			pipelineDesc.m_multiSampleState.m_sampleMask = 0xFFFFFFFF;

			pipelineDesc.m_depthStencilState.m_depthTestEnable = true;
			pipelineDesc.m_depthStencilState.m_depthWriteEnable = true;
			pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
			pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
			pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;

			VkPipelineColorBlendAttachmentState defaultBlendAttachment = {};
			defaultBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			defaultBlendAttachment.blendEnable = VK_FALSE;

			pipelineDesc.m_blendState.m_logicOpEnable = false;
			pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
			pipelineDesc.m_blendState.m_attachmentCount = 4;
			pipelineDesc.m_blendState.m_attachments[0] = defaultBlendAttachment;
			pipelineDesc.m_blendState.m_attachments[1] = defaultBlendAttachment;
			pipelineDesc.m_blendState.m_attachments[2] = defaultBlendAttachment;
			pipelineDesc.m_blendState.m_attachments[3] = defaultBlendAttachment;

			pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
			pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc, renderPassCompatDesc, 0, renderPass);

		VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			VkBuffer vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer.getBuffer();

			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition) }, VERTEX_POSITIONS_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { vertexBuffer, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * sizeof(VertexNormal) }, VERTEX_NORMALS_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord) }, VERTEX_TEXCOORDS_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);

			if (data.m_alphaMasked)
			{
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING);
			}

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

		VkDescriptorSet descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, data.m_alphaMasked ? 2 : 1, descriptorSets, 0, nullptr);

		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(width);
		viewport.height = static_cast<float>(height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { width, height };

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		vkCmdBindIndexBuffer(cmdBuf, data.m_passRecordContext->m_renderResources->m_indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT16);

		//VkBuffer vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer.getBuffer();
		//VkBuffer vertexBuffers[] = { vertexBuffer, vertexBuffer, vertexBuffer };
		//VkDeviceSize vertexBufferOffsets[] = { 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)) };
		//
		//vkCmdBindVertexBuffers(cmdBuf, 0, 3, vertexBuffers, vertexBufferOffsets);

		const glm::mat4 rowMajorViewMatrix = glm::transpose(data.m_passRecordContext->m_commonRenderData->m_viewMatrix);

		PushConsts pushConsts;
		pushConsts.jitteredViewProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredViewProjectionMatrix;
		pushConsts.viewMatrixRow0 = rowMajorViewMatrix[0];
		pushConsts.viewMatrixRow1 = rowMajorViewMatrix[1];
		pushConsts.viewMatrixRow2 = rowMajorViewMatrix[2];

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

		if (data.m_alphaMasked)
		{
			vkCmdDrawIndexedIndirect(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), data.m_drawOffset * sizeof(VkDrawIndexedIndirectCommand), data.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));
		}
		else
		{
			vkCmdDrawIndexedIndirectCountKHR(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), data.m_drawOffset * sizeof(VkDrawIndexedIndirectCommand), registry.getBuffer(data.m_drawCountBufferHandle), 0, data.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));
		}
		

		vkCmdEndRenderPass(cmdBuf);
	});
}
