#include "IrradianceVolumeDebugPass.h"
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
#include "../../../../../Application/Resources/Shaders/irradianceVolumeDebug_bindings.h"
}

void VEngine::IrradianceVolumeDebugPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_irradianceVolumeImageHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_irradianceVolumeAgeImageHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_depthImageHandle), ResourceState::WRITE_DEPTH_STENCIL},
		{ResourceViewHandle(data.m_colorImageHandle), ResourceState::WRITE_ATTACHMENT},
	};

	graph.addPass("Irradiance Volume Debug", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
		{
			const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
			const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

			// begin renderpass
			VkRenderPass renderPass;
			RenderPassCompatDesc renderPassCompatDesc;
			{
				RenderPassDesc renderPassDesc{};
				renderPassDesc.m_attachmentCount = 2;
				renderPassDesc.m_subpassCount = 1;
				renderPassDesc.m_attachments[0] = registry.getAttachmentDescription(data.m_depthImageHandle, ResourceState::WRITE_DEPTH_STENCIL);
				renderPassDesc.m_attachments[1] = registry.getAttachmentDescription(data.m_colorImageHandle, ResourceState::WRITE_ATTACHMENT);
				renderPassDesc.m_subpasses[0] = { 0, 1, true, 0 };
				renderPassDesc.m_subpasses[0].m_depthStencilAttachment = { 0, renderPassDesc.m_attachments[0].initialLayout };
				renderPassDesc.m_subpasses[0].m_colorAttachments[0] = { 1, renderPassDesc.m_attachments[1].initialLayout };

				data.m_passRecordContext->m_renderPassCache->getRenderPass(renderPassDesc, renderPassCompatDesc, renderPass);

				VkFramebuffer framebuffer;
				VkImageView attachmentViews[] =
				{
					registry.getImageView(data.m_depthImageHandle),
					registry.getImageView(data.m_colorImageHandle),
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

				VkClearValue clearValues[2];
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
			SpecEntry vertexShaderSpecEntries[]
			{
				SpecEntry(IRRADIANCE_VOLUME_WIDTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_WIDTH),
				SpecEntry(IRRADIANCE_VOLUME_HEIGHT_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_HEIGHT),
				SpecEntry(IRRADIANCE_VOLUME_DEPTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH),
			};

			SpecEntry fragmentShaderSpecEntries[]
			{
				SpecEntry(IRRADIANCE_VOLUME_CASCADES_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_CASCADES),
				SpecEntry(IRRADIANCE_VOLUME_WIDTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_WIDTH),
				SpecEntry(IRRADIANCE_VOLUME_HEIGHT_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_HEIGHT),
				SpecEntry(IRRADIANCE_VOLUME_DEPTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH),
				SpecEntry(IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH),
			};

			VkDynamicState dynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

			GraphicsPipelineDesc pipelineDesc;
			pipelineDesc.setVertexShader("Resources/Shaders/irradianceVolumeDebug_vert.spv", sizeof(vertexShaderSpecEntries) / sizeof(vertexShaderSpecEntries[0]), vertexShaderSpecEntries);
			pipelineDesc.setFragmentShader("Resources/Shaders/irradianceVolumeDebug_frag.spv", sizeof(fragmentShaderSpecEntries) / sizeof(fragmentShaderSpecEntries[0]), fragmentShaderSpecEntries);
			pipelineDesc.setVertexBindingDescription({ 0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX });
			pipelineDesc.setVertexAttributeDescription({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 });
			pipelineDesc.setPolygonModeCullMode(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
			pipelineDesc.setDepthTest(true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
			pipelineDesc.setColorBlendAttachment(GraphicsPipelineDesc::s_defaultBlendAttachment);
			pipelineDesc.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
			pipelineDesc.finalize();

			auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc, renderPassCompatDesc, 0, renderPass);

			VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

			// update descriptor sets
			{
				VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

				VkSampler linearSamplerRepeat = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX];

				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeImageHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, linearSamplerRepeat), IRRADIANCE_VOLUME_IMAGE_BINDING);
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeAgeImageHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, linearSamplerRepeat), AGE_IMAGE_BINDING);

				writer.commit();
			}

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

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

			vkCmdBindIndexBuffer(cmdBuf, data.m_passRecordContext->m_renderResources->m_lightProxyIndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT16);

			VkBuffer vertexBuffer = data.m_passRecordContext->m_renderResources->m_lightProxyVertexBuffer.getBuffer();
			VkDeviceSize vertexOffset = 0;
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer, &vertexOffset);

			PushConsts pushConsts;
			pushConsts.jitteredViewProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_jitteredViewProjectionMatrix;
			pushConsts.scale = RendererConsts::IRRADIANCE_VOLUME_BASE_SIZE * static_cast<float>(1 << data.m_cascadeIndex);
			pushConsts.cameraPosition = data.m_passRecordContext->m_commonRenderData->m_cameraPosition;
			pushConsts.cascadeIndex = data.m_cascadeIndex;
			pushConsts.showAge = data.m_showAge ? 1 : 0;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDrawIndexed(cmdBuf, 240, RendererConsts::IRRADIANCE_VOLUME_WIDTH *RendererConsts::IRRADIANCE_VOLUME_HEIGHT *RendererConsts::IRRADIANCE_VOLUME_DEPTH, 0, 0, 0);

			vkCmdEndRenderPass(cmdBuf);
		});
}
