#include "ImGuiPass.h"
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
#include "Graphics/imgui/imgui.h"

void VEngine::ImGuiPass::addToGraph(RenderGraph &graph, const Data &data)
{
	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	int fb_width = (int)(data.m_imGuiDrawData->DisplaySize.x * data.m_imGuiDrawData->FramebufferScale.x);
	int fb_height = (int)(data.m_imGuiDrawData->DisplaySize.y * data.m_imGuiDrawData->FramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0 || data.m_imGuiDrawData->TotalVtxCount == 0)
		return;

	BufferDescription vertexBufferDesc{};
	vertexBufferDesc.m_name = "ImGui Vertex Buffer";
	vertexBufferDesc.m_size = data.m_imGuiDrawData->TotalVtxCount * sizeof(ImDrawVert);
	vertexBufferDesc.m_hostVisible = true;

	BufferDescription indexBufferDesc{};
	indexBufferDesc.m_name = "ImGui Index Buffer";
	indexBufferDesc.m_size = data.m_imGuiDrawData->TotalIdxCount * sizeof(ImDrawIdx);
	indexBufferDesc.m_hostVisible = true;

	BufferViewHandle vertexBufferViewHandle = graph.createBufferView({ vertexBufferDesc.m_name, graph.createBuffer(vertexBufferDesc), 0, vertexBufferDesc.m_size });
	BufferViewHandle indexBufferViewHandle = graph.createBufferView({ indexBufferDesc.m_name, graph.createBuffer(indexBufferDesc), 0, indexBufferDesc.m_size });

	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_resultImageViewHandle), ResourceState::WRITE_ATTACHMENT},
		{ResourceViewHandle(vertexBufferViewHandle), ResourceState::READ_VERTEX_BUFFER},
		{ResourceViewHandle(indexBufferViewHandle), ResourceState::READ_INDEX_BUFFER},
	};

	graph.addPass("ImGui", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		// Upload vertex/index data into a single contiguous GPU buffer
		{
			size_t vertex_size = data.m_imGuiDrawData->TotalVtxCount * sizeof(ImDrawVert);
			size_t index_size = data.m_imGuiDrawData->TotalIdxCount * sizeof(ImDrawIdx);

			ImDrawVert* vtx_dst = NULL;
			ImDrawIdx* idx_dst = NULL;
			registry.map(vertexBufferViewHandle, (void**)(&vtx_dst));
			registry.map(indexBufferViewHandle, (void**)(&idx_dst));
			for (int n = 0; n < data.m_imGuiDrawData->CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = data.m_imGuiDrawData->CmdLists[n];
				memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
				memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
				vtx_dst += cmd_list->VtxBuffer.Size;
				idx_dst += cmd_list->IdxBuffer.Size;
			}
			registry.unmap(vertexBufferViewHandle);
			registry.unmap(indexBufferViewHandle);
		}


		// begin renderpass
		VkRenderPass renderPass;
		RenderPassCompatibilityDescription renderPassCompatDesc;
		{
			RenderPassDescription renderPassDesc{};
			renderPassDesc.m_attachmentCount = 1;
			renderPassDesc.m_subpassCount = 1;
			renderPassDesc.m_attachments[0] = registry.getAttachmentDescription(data.m_resultImageViewHandle, ResourceState::WRITE_ATTACHMENT);
			renderPassDesc.m_subpasses[0] = { 0, 1, false, 0 };
			renderPassDesc.m_subpasses[0].m_colorAttachments[0] = { 0, renderPassDesc.m_attachments[0].initialLayout };

			data.m_passRecordContext->m_renderPassCache->getRenderPass(renderPassDesc, renderPassCompatDesc, renderPass);

			VkFramebuffer framebuffer;
			VkImageView attachmentView = registry.getImageView(data.m_resultImageViewHandle);

			VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferCreateInfo.renderPass = renderPass;
			framebufferCreateInfo.attachmentCount = renderPassDesc.m_attachmentCount;
			framebufferCreateInfo.pAttachments = &attachmentView;
			framebufferCreateInfo.width = fb_width;
			framebufferCreateInfo.height = fb_height;
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
			renderPassBeginInfo.renderArea = { {}, {(uint32_t)fb_width, (uint32_t)fb_height} };
			renderPassBeginInfo.clearValueCount = renderPassDesc.m_attachmentCount;
			renderPassBeginInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		}

		// create pipeline description
		VKGraphicsPipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_vertexShaderStage.m_path, "Resources/Shaders/imgui_vert.spv");
			strcpy_s(pipelineDesc.m_fragmentShaderStage.m_path, "Resources/Shaders/imgui_frag.spv");

			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 1;
			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[0] = { 0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX };

			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 3;
			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32_SFLOAT, IM_OFFSETOF(ImDrawVert, pos) };
			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT, IM_OFFSETOF(ImDrawVert, uv) };
			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[2] = { 2, 0, VK_FORMAT_R8G8B8A8_UNORM, IM_OFFSETOF(ImDrawVert, col) };

			pipelineDesc.m_inputAssemblyState.m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			pipelineDesc.m_inputAssemblyState.m_primitiveRestartEnable = false;

			pipelineDesc.m_viewportState.m_viewportCount = 1;
			pipelineDesc.m_viewportState.m_viewports[0] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
			pipelineDesc.m_viewportState.m_scissorCount = 1;
			pipelineDesc.m_viewportState.m_scissors[0] = { {0, 0}, {1, 1} };

			pipelineDesc.m_rasterizationState.m_depthClampEnable = false;
			pipelineDesc.m_rasterizationState.m_rasterizerDiscardEnable = false;
			pipelineDesc.m_rasterizationState.m_polygonMode = VK_POLYGON_MODE_FILL;
			pipelineDesc.m_rasterizationState.m_cullMode = VK_CULL_MODE_NONE;
			pipelineDesc.m_rasterizationState.m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			pipelineDesc.m_rasterizationState.m_depthBiasEnable = false;
			pipelineDesc.m_rasterizationState.m_lineWidth = 1.0f;

			pipelineDesc.m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			pipelineDesc.m_multiSampleState.m_sampleShadingEnable = false;
			pipelineDesc.m_multiSampleState.m_sampleMask = 0xFFFFFFFF;

			pipelineDesc.m_depthStencilState.m_depthTestEnable = false;
			pipelineDesc.m_depthStencilState.m_depthWriteEnable = false;
			pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
			pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
			pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;

			VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
			colorBlendAttachment.blendEnable = VK_TRUE;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

			pipelineDesc.m_blendState.m_logicOpEnable = false;
			pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
			pipelineDesc.m_blendState.m_attachmentCount = 1;
			pipelineDesc.m_blendState.m_attachments[0] = colorBlendAttachment;

			pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
			pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc, renderPassCompatDesc, 0, renderPass);

		auto setupRenderState = [&]()
		{
			// Bind pipeline and descriptor sets:
			{
				vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);
				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 1, &data.m_passRecordContext->m_renderResources->m_imGuiDescriptorSet, 0, NULL);
			}

			// Bind Vertex And Index Buffer:
			{
				VkBuffer vertex_buffers[1] = { registry.getBuffer(vertexBufferViewHandle) };
				VkDeviceSize vertex_offset[1] = { 0 };
				vkCmdBindVertexBuffers(cmdBuf, 0, 1, vertex_buffers, vertex_offset);
				vkCmdBindIndexBuffer(cmdBuf, registry.getBuffer(indexBufferViewHandle), 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
			}

			// Setup viewport:
			{
				VkViewport viewport;
				viewport.x = 0;
				viewport.y = 0;
				viewport.width = (float)fb_width;
				viewport.height = (float)fb_height;
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
			}

			// Setup scale and translation:
			// Our visible imgui space lies from data.m_imGuiDrawData->DisplayPps (top left) to data.m_imGuiDrawData->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
			{
				float scale[2];
				scale[0] = 2.0f / data.m_imGuiDrawData->DisplaySize.x;
				scale[1] = 2.0f / data.m_imGuiDrawData->DisplaySize.y;
				float translate[2];
				translate[0] = -1.0f - data.m_imGuiDrawData->DisplayPos.x * scale[0];
				translate[1] = -1.0f - data.m_imGuiDrawData->DisplayPos.y * scale[1];
				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
			}
		};

		setupRenderState();


		// Will project scissor/clipping rectangles into framebuffer space
		ImVec2 clip_off = data.m_imGuiDrawData->DisplayPos;         // (0,0) unless using multi-viewports
		ImVec2 clip_scale = data.m_imGuiDrawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		// Render command lists
		// (Because we merged all buffers into a single one, we maintain our own offset into them)
		int global_vtx_offset = 0;
		int global_idx_offset = 0;
		for (int n = 0; n < data.m_imGuiDrawData->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = data.m_imGuiDrawData->CmdLists[n];
			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback != NULL)
				{
					// User callback, registered via ImDrawList::AddCallback()
					// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
					if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
						setupRenderState();
					else
						pcmd->UserCallback(cmd_list, pcmd);
				}
				else
				{
					// Project scissor/clipping rectangles into framebuffer space
					ImVec4 clip_rect;
					clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
					clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
					clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
					clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

					if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
					{
						// Negative offsets are illegal for vkCmdSetScissor
						if (clip_rect.x < 0.0f)
							clip_rect.x = 0.0f;
						if (clip_rect.y < 0.0f)
							clip_rect.y = 0.0f;

						// Apply scissor/clipping rectangle
						VkRect2D scissor;
						scissor.offset.x = (int32_t)(clip_rect.x);
						scissor.offset.y = (int32_t)(clip_rect.y);
						scissor.extent.width = (uint32_t)(clip_rect.z - clip_rect.x);
						scissor.extent.height = (uint32_t)(clip_rect.w - clip_rect.y);
						vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

						// Draw
						vkCmdDrawIndexed(cmdBuf, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
					}
				}
			}
			global_idx_offset += cmd_list->IdxBuffer.Size;
			global_vtx_offset += cmd_list->VtxBuffer.Size;
		}

		vkCmdEndRenderPass(cmdBuf);
	});
}
