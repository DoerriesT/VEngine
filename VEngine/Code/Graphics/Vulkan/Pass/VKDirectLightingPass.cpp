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
			strcpy_s(pipelineDesc.m_shaderStages.m_fragmentShaderPath, "Resources/Shaders/lighting_frag.spv");

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
			VkWriteDescriptorSet descriptorWrites[15] = {};
			uint32_t writeCount = 0;

			// depth image
			VkDescriptorImageInfo depthImageInfo = registry.getImageInfo(data.m_depthImageHandle);
			depthImageInfo.sampler = data.m_renderResources->m_pointSamplerClamp;
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = DEPTH_IMAGE_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[writeCount].pImageInfo = &depthImageInfo;
			++writeCount;

			// uv image
			VkDescriptorImageInfo uvImageInfo = registry.getImageInfo(data.m_uvImageHandle);
			uvImageInfo.sampler = data.m_renderResources->m_pointSamplerClamp;
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = UV_IMAGE_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[writeCount].pImageInfo = &uvImageInfo;
			++writeCount;

			// ddxy length image
			VkDescriptorImageInfo ddxyLengthImageInfo = registry.getImageInfo(data.m_ddxyLengthImageHandle);
			ddxyLengthImageInfo.sampler = data.m_renderResources->m_pointSamplerClamp;
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = DDXY_LENGTH_IMAGE_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[writeCount].pImageInfo = &ddxyLengthImageInfo;
			++writeCount;

			// ddxy rot & material id image
			VkDescriptorImageInfo ddxyRotMatIdImageInfo = registry.getImageInfo(data.m_ddxyRotMaterialIdImageHandle);
			ddxyRotMatIdImageInfo.sampler = data.m_renderResources->m_pointSamplerClamp;
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = DDXY_ROT_MATERIAL_ID_IMAGE_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[writeCount].pImageInfo = &ddxyRotMatIdImageInfo;
			++writeCount;

			// tangent space image
			VkDescriptorImageInfo tangentSpaceImageInfo = registry.getImageInfo(data.m_tangentSpaceImageHandle);
			tangentSpaceImageInfo.sampler = data.m_renderResources->m_pointSamplerClamp;
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = TANGENT_SPACE_IMAGE_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[writeCount].pImageInfo = &tangentSpaceImageInfo;
			++writeCount;

			// shadow atlas
			VkDescriptorImageInfo shadowAtlasImageInfo = registry.getImageInfo(data.m_shadowAtlasImageHandle);
			shadowAtlasImageInfo.sampler = data.m_renderResources->m_shadowSampler;
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = SHADOW_ATLAS_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[writeCount].pImageInfo = &shadowAtlasImageInfo;
			++writeCount;

			// directional light
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = DIRECTIONAL_LIGHT_DATA_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &data.m_directionalLightDataBufferInfo;
			++writeCount;

			// point light
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = POINT_LIGHT_DATA_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &data.m_pointLightDataBufferInfo;
			++writeCount;

			// shadow data
			VkDescriptorBufferInfo shadowDataBufferInfo = registry.getBufferInfo(data.m_shadowDataBufferHandle);
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = SHADOW_DATA_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &shadowDataBufferInfo;
			++writeCount;

			// point light z bins
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = POINT_LIGHT_Z_BINS_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &data.m_pointLightZBinsBufferInfo;
			++writeCount;

			// point light mask
			VkDescriptorBufferInfo pointLightMaskBufferInfo = registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle);
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = POINT_LIGHT_MASK_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &pointLightMaskBufferInfo;
			++writeCount;

			// material data
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = MATERIAL_DATA_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &data.m_materialDataBufferInfo;
			++writeCount;

			// shadow splits
			VkDescriptorBufferInfo shadowSplitsBufferInfo = registry.getBufferInfo(data.m_shadowSplitsBufferHandle);
			descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[writeCount].dstSet = descriptorSet;
			descriptorWrites[writeCount].dstBinding = SPLITS_BINDING;
			descriptorWrites[writeCount].dstArrayElement = 0;
			descriptorWrites[writeCount].descriptorCount = 1;
			descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[writeCount].pBufferInfo = &shadowSplitsBufferInfo;
			++writeCount;

			// occlusion image
			VkDescriptorImageInfo occlusionImageInfo = registry.getImageInfo(data.m_occlusionImageHandle);
			occlusionImageInfo.sampler = data.m_renderResources->m_pointSamplerClamp;
			if (data.m_ssao)
			{
				descriptorWrites[writeCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				descriptorWrites[writeCount].dstSet = descriptorSet;
				descriptorWrites[writeCount].dstBinding = OCCLUSION_IMAGE_BINDING;
				descriptorWrites[writeCount].dstArrayElement = 0;
				descriptorWrites[writeCount].descriptorCount = 1;
				descriptorWrites[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[writeCount].pImageInfo = &occlusionImageInfo;
				++writeCount;
			}

			vkUpdateDescriptorSets(g_context.m_device, writeCount, descriptorWrites, 0, nullptr);
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
