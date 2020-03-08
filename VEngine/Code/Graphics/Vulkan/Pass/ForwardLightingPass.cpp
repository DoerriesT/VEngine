#include "ForwardLightingPass.h"
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
#include "../../../../../Application/Resources/Shaders/forward_bindings.h"
}

void VEngine::ForwardLightingPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_indicesBufferHandle), ResourceState::READ_INDEX_BUFFER},
		{ResourceViewHandle(data.m_indirectBufferHandle), ResourceState::READ_INDIRECT_BUFFER},
		{ResourceViewHandle(data.m_depthImageViewHandle), ResourceState::READ_DEPTH_STENCIL},
		{ResourceViewHandle(data.m_resultImageViewHandle), ResourceState::WRITE_ATTACHMENT},
		{ResourceViewHandle(data.m_normalImageViewHandle), ResourceState::WRITE_ATTACHMENT},
		{ResourceViewHandle(data.m_specularRoughnessImageViewHandle), ResourceState::WRITE_ATTACHMENT},
		{ResourceViewHandle(data.m_pointLightBitMaskBufferHandle), ResourceState::READ_STORAGE_BUFFER_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_deferredShadowImageViewHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
	};

	graph.addPass("Forward Lighting", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// begin renderpass
			VkRenderPass renderPass;
			RenderPassCompatDesc renderPassCompatDesc;
			{
				RenderPassDesc renderPassDesc{};
				renderPassDesc.m_attachmentCount = 4;
				renderPassDesc.m_subpassCount = 1;
				renderPassDesc.m_attachments[0] = registry.getAttachmentDescription(data.m_depthImageViewHandle, ResourceState::READ_DEPTH_STENCIL);
				renderPassDesc.m_attachments[1] = registry.getAttachmentDescription(data.m_resultImageViewHandle, ResourceState::WRITE_ATTACHMENT);
				renderPassDesc.m_attachments[2] = registry.getAttachmentDescription(data.m_normalImageViewHandle, ResourceState::WRITE_ATTACHMENT);
				renderPassDesc.m_attachments[3] = registry.getAttachmentDescription(data.m_specularRoughnessImageViewHandle, ResourceState::WRITE_ATTACHMENT);
				renderPassDesc.m_subpasses[0] = { 0, 3, true, 0 };
				renderPassDesc.m_subpasses[0].m_colorAttachments[0] = { 1, renderPassDesc.m_attachments[1].initialLayout };
				renderPassDesc.m_subpasses[0].m_colorAttachments[1] = { 2, renderPassDesc.m_attachments[2].initialLayout };
				renderPassDesc.m_subpasses[0].m_colorAttachments[2] = { 3, renderPassDesc.m_attachments[3].initialLayout };
				renderPassDesc.m_subpasses[0].m_depthStencilAttachment = { 0, renderPassDesc.m_attachments[0].initialLayout };

				data.m_passRecordContext->m_renderPassCache->getRenderPass(renderPassDesc, renderPassCompatDesc, renderPass);

				VkFramebuffer framebuffer;
				VkImageView attachmentViews[] =
				{
					registry.getImageView(data.m_depthImageViewHandle),
					registry.getImageView(data.m_resultImageViewHandle),
					registry.getImageView(data.m_normalImageViewHandle),
					registry.getImageView(data.m_specularRoughnessImageViewHandle),
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

				VkClearValue clearValues[4];
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
			SpecEntry specEntries[]
			{
				SpecEntry(DIRECTIONAL_LIGHT_COUNT_CONST_ID, glm::min(data.m_passRecordContext->m_commonRenderData->m_directionalLightCount, 4u)),
				SpecEntry(WIDTH_CONST_ID, width),
				SpecEntry(HEIGHT_CONST_ID, height),
				SpecEntry(TEXEL_WIDTH_CONST_ID, 1.0f / width),
				SpecEntry(TEXEL_HEIGHT_CONST_ID, 1.0f / height),
			};

			VkPipelineColorBlendAttachmentState colorBlendAttachments[]
			{
				GraphicsPipelineDesc::s_defaultBlendAttachment,
				GraphicsPipelineDesc::s_defaultBlendAttachment,
				GraphicsPipelineDesc::s_defaultBlendAttachment,
			};

			VkDynamicState dynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

			GraphicsPipelineDesc pipelineDesc;
			pipelineDesc.setVertexShader("Resources/Shaders/forward_vert.spv");
			pipelineDesc.setFragmentShader("Resources/Shaders/forward_frag.spv", sizeof(specEntries) / sizeof(specEntries[0]), specEntries);
			pipelineDesc.setPolygonModeCullMode(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
			pipelineDesc.setDepthTest(true, false, VK_COMPARE_OP_EQUAL);
			pipelineDesc.setColorBlendAttachments(sizeof(colorBlendAttachments) / sizeof(colorBlendAttachments[0]), colorBlendAttachments);
			pipelineDesc.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc, renderPassCompatDesc, 0, renderPass);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
				VkSampler linearSamplerRepeat = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX];

				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				VkBuffer vertexBuffer = data.m_passRecordContext->m_renderResources->m_vertexBuffer.getBuffer();

				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { vertexBuffer, 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition) }, VERTEX_POSITIONS_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { vertexBuffer, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * sizeof(VertexNormal) }, VERTEX_NORMALS_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, { vertexBuffer, RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)), RendererConsts::MAX_VERTICES * sizeof(VertexTexCoord) }, VERTEX_TEXCOORDS_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_subMeshInfoBufferInfo, SUB_MESH_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, registry.getImageInfo(data.m_deferredShadowImageViewHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER), DEFERRED_SHADOW_IMAGE_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_directionalLightDataBufferInfo, DIRECTIONAL_LIGHT_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_pointLightDataBufferInfo, POINT_LIGHT_DATA_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_pointLightZBinsBufferInfo, POINT_LIGHT_Z_BINS_BINDING);
				writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle), POINT_LIGHT_MASK_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_SAMPLER, { pointSamplerClamp }, POINT_SAMPLER_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

			VkDescriptorSet descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 2, descriptorSets, 0, nullptr);

			VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
			VkRect2D scissor{ { 0, 0 }, { width, height } };

			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
			vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

			vkCmdBindIndexBuffer(cmdBuf, registry.getBuffer(data.m_indicesBufferHandle), 0, VK_INDEX_TYPE_UINT32);

			const glm::mat4 rowMajorViewMatrix = glm::transpose(data.m_passRecordContext->m_commonRenderData->m_viewMatrix);

			PushConsts pushConsts;
			pushConsts.jitteredViewProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredViewProjectionMatrix;
			pushConsts.viewMatrixRow0 = rowMajorViewMatrix[0];
			pushConsts.viewMatrixRow1 = rowMajorViewMatrix[1];
			pushConsts.viewMatrixRow2 = rowMajorViewMatrix[2];
			pushConsts.pointLightCount = data.m_passRecordContext->m_commonRenderData->m_pointLightCount;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDrawIndexedIndirect(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), 0, 1, sizeof(VkDrawIndexedIndirectCommand));

			vkCmdEndRenderPass(cmdBuf);
		});
}
