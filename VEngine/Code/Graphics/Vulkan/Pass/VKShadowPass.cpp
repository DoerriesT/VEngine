#include "VKShadowPass.h"
#include "Graphics/LightData.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Mesh.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/Vulkan/RenderPassCache.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/DeferredObjectDeleter.h"
#include "Utility/Utility.h"
#include "GlobalVar.h"

namespace
{
	using vec4 = glm::vec4;
	using mat4 = glm::mat4;
	using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/shadows_bindings.h"
}

void VEngine::VKShadowPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_shadowDataBufferHandle), ResourceState::READ_STORAGE_BUFFER_VERTEX_SHADER},
		{ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::READ_INDIRECT_BUFFER},
		{ResourceViewHandle(data.m_shadowAtlasImageHandle), ResourceState::WRITE_DEPTH_STENCIL},
	};

	graph.addPass(data.m_alphaMasked ? "Shadows Alpha" : "Shadows", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		const uint32_t width = g_shadowAtlasSize;
		const uint32_t height = g_shadowAtlasSize;

		// create pipeline description
		VKGraphicsPipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_vertexShaderStage.m_path, data.m_alphaMasked ? "Resources/Shaders/shadows_alpha_mask_vert.spv" : "Resources/Shaders/shadows_vert.spv");

			if (data.m_alphaMasked)
			{
				strcpy_s(pipelineDesc.m_fragmentShaderStage.m_path, "Resources/Shaders/shadows_frag.spv");
			}

			if (data.m_alphaMasked)
			{
				pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 2;
				pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[0] = { 0, sizeof(VertexPosition), VK_VERTEX_INPUT_RATE_VERTEX };
				pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[1] = { 1, sizeof(VertexTexCoord), VK_VERTEX_INPUT_RATE_VERTEX };

				pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 2;
				pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
				pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[1] = { 1, 1, VK_FORMAT_R32G32_SFLOAT, 0 };
			}
			else
			{
				pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 1;
				pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[0] = { 0, sizeof(VertexPosition), VK_VERTEX_INPUT_RATE_VERTEX };

				pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 1;
				pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
			}

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
			pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
			pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;

			pipelineDesc.m_blendState.m_logicOpEnable = false;
			pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
			pipelineDesc.m_blendState.m_attachmentCount = 0;

			pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
			pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

			pipelineDesc.finalize();
		}

		// begin renderpass
		VkRenderPass renderPass;
		RenderPassCompatibilityDescription renderPassCompatDesc;
		{
			RenderPassDescription renderPassDesc{};
			renderPassDesc.m_attachmentCount = 1;
			renderPassDesc.m_subpassCount = 1;
			renderPassDesc.m_attachments[0] = registry.getAttachmentDescription(data.m_shadowAtlasImageHandle, ResourceState::WRITE_DEPTH_STENCIL);
			renderPassDesc.m_subpasses[0] = { 0, 0, true, 0 };
			renderPassDesc.m_subpasses[0].m_depthStencilAttachment = { 0, renderPassDesc.m_attachments[0].initialLayout };

			data.m_passRecordContext->m_renderPassCache->getRenderPass(renderPassDesc, renderPassCompatDesc, renderPass);

			VkFramebuffer framebuffer;
			VkImageView attachmentView = registry.getImageView(data.m_shadowAtlasImageHandle);

			VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferCreateInfo.renderPass = renderPass;
			framebufferCreateInfo.attachmentCount = renderPassDesc.m_attachmentCount;
			framebufferCreateInfo.pAttachments = &attachmentView;
			framebufferCreateInfo.width = width;
			framebufferCreateInfo.height = height;
			framebufferCreateInfo.layers = 1;

			if (vkCreateFramebuffer(g_context.m_device, &framebufferCreateInfo, nullptr, &framebuffer) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create framebuffer!", EXIT_FAILURE);
			}

			data.m_passRecordContext->m_deferredObjectDeleter->add(framebuffer);

			VkClearValue clearValues[1];

			VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = framebuffer;
			renderPassBeginInfo.renderArea = { {}, {width, height} };
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		}

		auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc, renderPassCompatDesc, 0, renderPass);

		VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// instance data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING);

			// transform data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);

			// shadow data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_shadowDataBufferHandle), SHADOW_DATA_BINDING);

			if (data.m_alphaMasked)
			{
				// material data
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING);
			}

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

		if (data.m_alphaMasked)
		{
			VkDescriptorSet descriptorSets[] = { descriptorSet ,  data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 2, descriptorSets, 0, nullptr);
		}
		else
		{
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);
		}

		VkViewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		// clear the relevant areas in the atlas
		if (data.m_clear)
		{
			VkClearRect clearRects[8];

			VkClearAttachment clearAttachment = {};
			clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			clearAttachment.clearValue.depthStencil = { 1.0f, 0 };

			for (size_t i = 0; i < data.m_shadowJobCount; ++i)
			{
				const ShadowJob &job = data.m_shadowJobs[i];

				clearRects[i].rect = { { int32_t(job.m_offsetX), int32_t(job.m_offsetY) },{ job.m_size, job.m_size } };
				clearRects[i].baseArrayLayer = 0;
				clearRects[i].layerCount = 1;
			}

			vkCmdClearAttachments(cmdBuf, 1, &clearAttachment, data.m_shadowJobCount, clearRects);
		}

		vkCmdBindIndexBuffer(cmdBuf, data.m_passRecordContext->m_renderResources->m_indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT16);

		VkBuffer vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer.getBuffer();
		VkBuffer vertexBuffers[] = { vertexBuffer, vertexBuffer };
		VkDeviceSize vertexBufferOffsets[] = { 0, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)) };

		vkCmdBindVertexBuffers(cmdBuf, 0, data.m_alphaMasked ? 2 : 1, vertexBuffers, vertexBufferOffsets);

		for (size_t i = 0; i < data.m_shadowJobCount; ++i)
		{
			const ShadowJob &job = data.m_shadowJobs[i];

			// set viewport and scissor
			VkViewport viewport = { static_cast<float>(job.m_offsetX), static_cast<float>(job.m_offsetY), static_cast<float>(job.m_size), static_cast<float>(job.m_size), 0.0f, 1.0f };
			VkRect2D scissor = { { static_cast<int32_t>(job.m_offsetX), static_cast<int32_t>(job.m_offsetY) }, { job.m_size, job.m_size } };

			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
			vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

			uint32_t shadowDataIndex = static_cast<uint32_t>(i);
			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(shadowDataIndex), &shadowDataIndex);

			vkCmdDrawIndexedIndirect(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), 0, data.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));
		}

		vkCmdEndRenderPass(cmdBuf);
	});
}
