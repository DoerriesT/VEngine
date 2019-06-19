#include "VKDirectLightingPass.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/RenderData.h"

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/lighting_bindings.h"
}

void VEngine::VKDirectLightingPass::addToGraph(FrameGraph::Graph & graph, const Data & data)
{
	graph.addGraphicsPass("Direct Lighting", data.m_width, data.m_height,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readStorageBuffer(data.m_pointLightBitMaskBufferHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readStorageBuffer(data.m_shadowDataBufferHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readStorageBuffer(data.m_shadowSplitsBufferHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readTexture(data.m_depthImageHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readTexture(data.m_uvImageHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readTexture(data.m_ddxyLengthImageHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readTexture(data.m_ddxyRotMaterialIdImageHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readTexture(data.m_tangentSpaceImageHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readTexture(data.m_shadowAtlasImageHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		if (data.m_ssao)
		{
			builder.readTexture(data.m_occlusionImageHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}

		builder.writeColorAttachment(data.m_resultImageHandle, OUT_RESULT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKGraphicsPipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/lighting_vert.spv");
			strcpy_s(pipelineDesc.m_shaderStages.m_fragmentShaderPath, data.m_ssao ? "Resources/Shaders/lighting_SSAO_ENABLED_frag.spv" : "Resources/Shaders/lighting_frag.spv");

			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 0;

			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 0;

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

			VkPipelineColorBlendAttachmentState defaultBlendAttachment = {};
			defaultBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			defaultBlendAttachment.blendEnable = VK_FALSE;

			pipelineDesc.m_blendState.m_logicOpEnable = false;
			pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
			pipelineDesc.m_blendState.m_attachmentCount = 1;
			pipelineDesc.m_blendState.m_attachments[0] = defaultBlendAttachment;

			pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
			pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc, *renderPassDescription, renderPass);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkSampler pointSamplerClamp = data.m_renderResources->m_pointSamplerClamp;

			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// depth image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_depthImageHandle, pointSamplerClamp), DEPTH_IMAGE_BINDING);

			// uv image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_uvImageHandle, pointSamplerClamp), UV_IMAGE_BINDING);

			// ddxy length image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_ddxyLengthImageHandle, pointSamplerClamp), DDXY_LENGTH_IMAGE_BINDING);

			// ddxy rot & material id image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_ddxyRotMaterialIdImageHandle, pointSamplerClamp), DDXY_ROT_MATERIAL_ID_IMAGE_BINDING);

			// tangent space image
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_tangentSpaceImageHandle, pointSamplerClamp), TANGENT_SPACE_IMAGE_BINDING);

			// shadow atlas
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_shadowAtlasImageHandle, data.m_renderResources->m_shadowSampler), SHADOW_ATLAS_BINDING);

			// directional light
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_directionalLightDataBufferInfo, DIRECTIONAL_LIGHT_DATA_BINDING);

			// point light
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_pointLightDataBufferInfo, POINT_LIGHT_DATA_BINDING);

			// shadow data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_shadowDataBufferHandle), SHADOW_DATA_BINDING);

			// point light z bins
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_pointLightZBinsBufferInfo, POINT_LIGHT_Z_BINS_BINDING);

			// point light mask
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle), POINT_LIGHT_MASK_BINDING);

			// material data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING);

			// shadow splits
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_shadowSplitsBufferHandle), SPLITS_BINDING);

			if (data.m_ssao)
			{
				// occlusion image
				writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_occlusionImageHandle, pointSamplerClamp), OCCLUSION_IMAGE_BINDING);
			}

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

		VkDescriptorSet descriptorSets[] = { descriptorSet, data.m_renderResources->m_textureDescriptorSet };
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 2, descriptorSets, 0, nullptr);

		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(data.m_width);
		viewport.height = static_cast<float>(data.m_height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { data.m_width, data.m_height };

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		const glm::mat4 rowMajorInvViewMatrix = glm::transpose(data.m_commonRenderData->m_invViewMatrix);

		PushConsts pushConsts;
		pushConsts.invJitteredProjectionMatrix = data.m_commonRenderData->m_invJitteredProjectionMatrix;
		pushConsts.invViewMatrixRow0 = rowMajorInvViewMatrix[0];
		pushConsts.invViewMatrixRow1 = rowMajorInvViewMatrix[1];
		pushConsts.invViewMatrixRow2 = rowMajorInvViewMatrix[2];
		pushConsts.pointLightCount = data.m_commonRenderData->m_pointLightCount;
		pushConsts.directionalLightCount = data.m_commonRenderData->m_directionalLightCount;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

		vkCmdDrawIndexed(cmdBuf, 6, 1, 0, 0, 0);
	});
}
