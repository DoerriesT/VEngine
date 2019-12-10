#include "VKDirectLightingPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
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
#include "../../../../../Application/Resources/Shaders/lighting_bindings.h"
}

void VEngine::VKDirectLightingPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_resultImageHandle), ResourceState::WRITE_ATTACHMENT},
		{ResourceViewHandle(data.m_pointLightBitMaskBufferHandle), ResourceState::READ_STORAGE_BUFFER_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_depthImageHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_uvImageHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_ddxyLengthImageHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_ddxyRotMaterialIdImageHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_tangentSpaceImageHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
		{ResourceViewHandle(data.m_deferredShadowImageViewHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
		{ ResourceViewHandle(data.m_irradianceVolumeImageHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER },
		{ ResourceViewHandle(data.m_irradianceVolumeDepthImageHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER },
		{ResourceViewHandle(data.m_occlusionImageHandle), ResourceState::READ_TEXTURE_FRAGMENT_SHADER},
	};
	const uint32_t usageCount = data.m_ssao ? sizeof(passUsages) / sizeof(passUsages[0]) : sizeof(passUsages) / sizeof(passUsages[0]) - 1;

	graph.addPass("Direct Lighting", QueueType::GRAPHICS, usageCount, passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		SpecEntry fragmentSpecEntries[]
		{
			SpecEntry(DIRECTIONAL_LIGHT_COUNT_CONST_ID, data.m_passRecordContext->m_commonRenderData->m_directionalLightCount),
			SpecEntry(IRRADIANCE_VOLUME_WIDTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_WIDTH),
			SpecEntry(IRRADIANCE_VOLUME_HEIGHT_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_HEIGHT),
			SpecEntry(IRRADIANCE_VOLUME_DEPTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH),
			SpecEntry(IRRADIANCE_VOLUME_BASE_SCALE_CONST_ID, 1.0f / RendererConsts::IRRADIANCE_VOLUME_BASE_SIZE),
			SpecEntry(IRRADIANCE_VOLUME_CASCADES_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_CASCADES),
			SpecEntry(IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH),
			SpecEntry(IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH_CONST_ID, RendererConsts::IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH),
		};

		VkDynamicState dynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.setVertexShader("Resources/Shaders/lighting_vert.spv");
		pipelineDesc.setFragmentShader(data.m_ssao ? "Resources/Shaders/lighting_SSAO_ENABLED_frag.spv" : "Resources/Shaders/lighting_frag.spv", sizeof(fragmentSpecEntries) / sizeof(fragmentSpecEntries[0]), fragmentSpecEntries);
		pipelineDesc.setColorBlendAttachment(GraphicsPipelineDesc::s_defaultBlendAttachment);
		pipelineDesc.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
		pipelineDesc.finalize();

		// begin renderpass
		VkRenderPass renderPass;
		RenderPassCompatDesc renderPassCompatDesc;
		{
			RenderPassDesc renderPassDesc{};
			renderPassDesc.m_attachmentCount = 1;
			renderPassDesc.m_subpassCount = 1;
			renderPassDesc.m_attachments[0] = registry.getAttachmentDescription(data.m_resultImageHandle, ResourceState::WRITE_ATTACHMENT);
			renderPassDesc.m_subpasses[0] = { 0, 1, false, 0 };
			renderPassDesc.m_subpasses[0].m_colorAttachments[0] = { 0, renderPassDesc.m_attachments[0].initialLayout };

			data.m_passRecordContext->m_renderPassCache->getRenderPass(renderPassDesc, renderPassCompatDesc, renderPass);

			VkFramebuffer framebuffer;
			VkImageView colorAttachmentView = registry.getImageView(data.m_resultImageHandle);

			VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferCreateInfo.renderPass = renderPass;
			framebufferCreateInfo.attachmentCount = renderPassDesc.m_attachmentCount;
			framebufferCreateInfo.pAttachments = &colorAttachmentView;
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
			VkSampler pointSamplerClamp = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_POINT_CLAMP_IDX];
			VkSampler linearSamplerRepeat = data.m_passRecordContext->m_renderResources->m_samplers[RendererConsts::SAMPLER_LINEAR_REPEAT_IDX];

			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_depthImageHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, pointSamplerClamp), DEPTH_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_uvImageHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, pointSamplerClamp), UV_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_ddxyLengthImageHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, pointSamplerClamp), DDXY_LENGTH_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_ddxyRotMaterialIdImageHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, pointSamplerClamp), DDXY_ROT_MATERIAL_ID_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_tangentSpaceImageHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, pointSamplerClamp), TANGENT_SPACE_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_deferredShadowImageViewHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, pointSamplerClamp), DEFERRED_SHADOW_IMAGE_BINDING);
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeImageHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, linearSamplerRepeat), IRRADIANCE_VOLUME_IMAGE_BINDING);
			//writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_irradianceVolumeDepthImageHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, linearSamplerRepeat), IRRADIANCE_VOLUME_DEPTH_IMAGE_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_directionalLightDataBufferInfo, DIRECTIONAL_LIGHT_DATA_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_pointLightDataBufferInfo, POINT_LIGHT_DATA_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_pointLightZBinsBufferInfo, POINT_LIGHT_Z_BINS_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle), POINT_LIGHT_MASK_BINDING);
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING);

			if (data.m_ssao)
			{
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_occlusionImageHandle, ResourceState::READ_TEXTURE_FRAGMENT_SHADER, pointSamplerClamp), OCCLUSION_IMAGE_BINDING);
			}

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

		VkDescriptorSet descriptorSets[] = { descriptorSet, data.m_passRecordContext->m_renderResources->m_textureDescriptorSet };
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 2, descriptorSets, 0, nullptr);

		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(data.m_passRecordContext->m_commonRenderData->m_width);
		viewport.height = static_cast<float>(data.m_passRecordContext->m_commonRenderData->m_height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { data.m_passRecordContext->m_commonRenderData->m_width, data.m_passRecordContext->m_commonRenderData->m_height };

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		mat4 transposedInvViewMatrix = glm::transpose(data.m_passRecordContext->m_commonRenderData->m_invViewMatrix);

		PushConsts pushConsts;
		pushConsts.invJitteredProjectionMatrix = data.m_passRecordContext->m_commonRenderData->m_invJitteredProjectionMatrix;
		pushConsts.invViewMatrixRow0 = transposedInvViewMatrix[0];
		pushConsts.invViewMatrixRow1 = transposedInvViewMatrix[1];
		pushConsts.invViewMatrixRow2 = transposedInvViewMatrix[2];
		pushConsts.pointLightCount = data.m_passRecordContext->m_commonRenderData->m_pointLightCount;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

		vkCmdDraw(cmdBuf, 6, 1, 0, 0);

		vkCmdEndRenderPass(cmdBuf);
	});
}
