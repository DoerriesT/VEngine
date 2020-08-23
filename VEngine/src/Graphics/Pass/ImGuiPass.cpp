#include "ImGuiPass.h"
#include "Graphics/RenderResources.h"
#include "Graphics/Mesh.h"
#include "Graphics/PipelineCache.h"
#include "Graphics/DescriptorSetCache.h"
#include "Graphics/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/imgui/imgui.h"
#include "Graphics/gal/Initializers.h"

using namespace VEngine::gal;

void VEngine::ImGuiPass::addToGraph(rg::RenderGraph &graph, const Data &data)
{
	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	int fb_width = (int)(data.m_imGuiDrawData->DisplaySize.x * data.m_imGuiDrawData->FramebufferScale.x);
	int fb_height = (int)(data.m_imGuiDrawData->DisplaySize.y * data.m_imGuiDrawData->FramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0 || data.m_imGuiDrawData->TotalVtxCount == 0)
		return;

	rg::BufferDescription vertexBufferDesc{};
	vertexBufferDesc.m_name = "ImGui Vertex Buffer";
	vertexBufferDesc.m_size = data.m_imGuiDrawData->TotalVtxCount * sizeof(ImDrawVert);
	vertexBufferDesc.m_hostVisible = true;

	rg::BufferDescription indexBufferDesc{};
	indexBufferDesc.m_name = "ImGui Index Buffer";
	indexBufferDesc.m_size = data.m_imGuiDrawData->TotalIdxCount * sizeof(ImDrawIdx);
	indexBufferDesc.m_hostVisible = true;

	rg::BufferViewHandle vertexBufferViewHandle = graph.createBufferView({ vertexBufferDesc.m_name, graph.createBuffer(vertexBufferDesc), 0, vertexBufferDesc.m_size });
	rg::BufferViewHandle indexBufferViewHandle = graph.createBufferView({ indexBufferDesc.m_name, graph.createBuffer(indexBufferDesc), 0, indexBufferDesc.m_size });

	rg::ResourceUsageDescription passUsages[]
	{
		{rg::ResourceViewHandle(data.m_resultImageViewHandle), {gal::ResourceState::WRITE_ATTACHMENT, PipelineStageFlagBits::COLOR_ATTACHMENT_OUTPUT_BIT}},
		{rg::ResourceViewHandle(vertexBufferViewHandle), {gal::ResourceState::READ_VERTEX_BUFFER, PipelineStageFlagBits::VERTEX_INPUT_BIT}},
		{rg::ResourceViewHandle(indexBufferViewHandle), {gal::ResourceState::READ_INDEX_BUFFER, PipelineStageFlagBits::VERTEX_INPUT_BIT}},
	};

	graph.addPass("ImGui", rg::QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](CommandList *cmdList, const rg::Registry &registry)
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

		GraphicsPipelineCreateInfo pipelineCreateInfo;
		GraphicsPipelineBuilder builder(pipelineCreateInfo);

		VertexInputAttributeDescription attributeDescs[]
		{
			{ "POSITION", 0, 0, Format::R32G32_SFLOAT, IM_OFFSETOF(ImDrawVert, pos) },
			{ "TEXCOORD0", 1, 0, Format::R32G32_SFLOAT, IM_OFFSETOF(ImDrawVert, uv) },
			{ "COLOR0", 2, 0, Format::R8G8B8A8_UNORM, IM_OFFSETOF(ImDrawVert, col) },
		};

		PipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.m_blendEnable = true;
		colorBlendAttachment.m_srcColorBlendFactor = BlendFactor::SRC_ALPHA;
		colorBlendAttachment.m_dstColorBlendFactor = BlendFactor::ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.m_colorBlendOp = BlendOp::ADD;
		colorBlendAttachment.m_srcAlphaBlendFactor = BlendFactor::ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.m_dstAlphaBlendFactor = BlendFactor::ZERO;
		colorBlendAttachment.m_alphaBlendOp = BlendOp::ADD;
		colorBlendAttachment.m_colorWriteMask = ColorComponentFlagBits::R_BIT | ColorComponentFlagBits::G_BIT | ColorComponentFlagBits::B_BIT | ColorComponentFlagBits::A_BIT;

		builder.setVertexShader("Resources/Shaders/hlsl/imgui_vs");
		builder.setFragmentShader("Resources/Shaders/hlsl/imgui_ps");
		builder.setVertexBindingDescription({ 0, sizeof(ImDrawVert), VertexInputRate::VERTEX });
		builder.setVertexAttributeDescriptions((uint32_t)std::size(attributeDescs), attributeDescs);
		builder.setColorBlendAttachment(colorBlendAttachment);
		builder.setDynamicState(DynamicStateFlagBits::VIEWPORT_BIT | DynamicStateFlagBits::SCISSOR_BIT);
		builder.setColorAttachmentFormat(registry.getImageView(data.m_resultImageViewHandle)->getImage()->getDescription().m_format);

		auto pipeline = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineCreateInfo);

		// begin renderpass
		ColorAttachmentDescription colorAttachDesc{ registry.getImageView(data.m_resultImageViewHandle), data.m_clear ? AttachmentLoadOp::CLEAR : AttachmentLoadOp::LOAD, AttachmentStoreOp::STORE, {} };
		cmdList->beginRenderPass(1, &colorAttachDesc, nullptr, { {}, {(uint32_t)fb_width, (uint32_t)fb_height} });

		auto setupRenderState = [&]()
		{
			// Bind pipeline and descriptor sets:
			{
				cmdList->bindPipeline(pipeline);
				DescriptorSet *descriptorSets[] = {data.m_passRecordContext->m_renderResources->m_textureDescriptorSet, data.m_passRecordContext->m_renderResources->m_samplerDescriptorSet };
				cmdList->bindDescriptorSets(pipeline, 0, 2, descriptorSets);
			}

			// Bind Vertex And Index Buffer:
			{
				Buffer *vertex_buffers[1] = { registry.getBuffer(vertexBufferViewHandle) };
				uint64_t vertex_offset[1] = { 0 };
				cmdList->bindVertexBuffers(0, 1, vertex_buffers, vertex_offset);
				cmdList->bindIndexBuffer(registry.getBuffer(indexBufferViewHandle), 0, sizeof(ImDrawIdx) == 2 ? IndexType::UINT16 : IndexType::UINT32);
			}

			// Setup viewport:
			{
				Viewport viewport{ 0.0f, 0.0f, static_cast<float>(fb_width), static_cast<float>(fb_height), 0.0f, 1.0f };

				cmdList->setViewport(0, 1, &viewport);
			}

			// Setup scale and translation:
			// Our visible imgui space lies from data.m_imGuiDrawData->DisplayPps (top left) to data.m_imGuiDrawData->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
			{
#if 0
				// for vulkan ndc space (y down)
				float scale[2];
				scale[0] = 2.0f / data.m_imGuiDrawData->DisplaySize.x;
				scale[1] = 2.0f / data.m_imGuiDrawData->DisplaySize.y;
				float translate[2];
				translate[0] = -1.0f - data.m_imGuiDrawData->DisplayPos.x * scale[0];
				translate[1] = -1.0f - data.m_imGuiDrawData->DisplayPos.y * scale[1];
#else
				// for directx-style ndc space (y up)
				float scale[2];
				scale[0] = (1.0f / (data.m_imGuiDrawData->DisplaySize.x - data.m_imGuiDrawData->DisplayPos.x)) * 2.0f;
				scale[1] = (1.0f / (data.m_imGuiDrawData->DisplaySize.y - data.m_imGuiDrawData->DisplayPos.y)) * -2.0f;
				float translate[2];
				translate[0] = -1.0f;
				translate[1] = 1.0f;
#endif
				cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
				cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
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
						Rect scissor;
						scissor.m_offset.m_x = (int32_t)(clip_rect.x);
						scissor.m_offset.m_y = (int32_t)(clip_rect.y);
						scissor.m_extent.m_width = (uint32_t)(clip_rect.z - clip_rect.x);
						scissor.m_extent.m_height = (uint32_t)(clip_rect.w - clip_rect.y);
						cmdList->setScissor(0, 1, &scissor);

						// Draw
						uint32_t textureIndex = (uint32_t)pcmd->TextureId;
						assert(textureIndex != 0);
						textureIndex -= 1;

						cmdList->pushConstants(pipeline, ShaderStageFlagBits::VERTEX_BIT, 4 * sizeof(float), sizeof(textureIndex), &textureIndex);
						cmdList->drawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
					}
				}
			}
			global_idx_offset += cmd_list->IdxBuffer.Size;
			global_vtx_offset += cmd_list->VtxBuffer.Size;
		}

		cmdList->endRenderPass();
	});
}
