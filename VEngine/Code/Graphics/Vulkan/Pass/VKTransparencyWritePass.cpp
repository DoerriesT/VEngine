#include "VKTransparencyWritePass.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Mesh.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"

namespace
{
	using vec4 = glm::vec4;
	using mat4 = glm::mat4;
	using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/transparencyWrite_bindings.h"
}

void VEngine::VKTransparencyWritePass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addGraphicsPass("Transparency Write", data.m_width, data.m_height,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readIndirectBuffer(data.m_indirectBufferHandle);
		builder.readStorageBuffer(data.m_pointLightBitMaskBufferHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readTexture(data.m_shadowAtlasImageHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		builder.readDepthStencil(data.m_depthImageHandle);
		builder.writeColorAttachment(data.m_lightImageHandle, 0);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKGraphicsPipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/transparencyWrite_vert.spv");
			strcpy_s(pipelineDesc.m_shaderStages.m_fragmentShaderPath,"Resources/Shaders/transparencyWrite_frag.spv");

			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 3;
			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[0] = { 0, sizeof(VertexPosition), VK_VERTEX_INPUT_RATE_VERTEX };
			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[1] = { 1, sizeof(VertexNormal), VK_VERTEX_INPUT_RATE_VERTEX };
			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[2] = { 2, sizeof(VertexTexCoord), VK_VERTEX_INPUT_RATE_VERTEX };

			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 3;
			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[1] = { 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 };
			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[2] = { 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 };

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

			pipelineDesc.m_depthStencilState.m_depthTestEnable = true;
			pipelineDesc.m_depthStencilState.m_depthWriteEnable = false;
			pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
			pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
			pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;

			VkPipelineColorBlendAttachmentState overBlendAttachment{};
			overBlendAttachment.blendEnable = VK_TRUE;
			overBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			overBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			overBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			overBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			overBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			overBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
			overBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;


			pipelineDesc.m_blendState.m_logicOpEnable = false;
			pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
			pipelineDesc.m_blendState.m_attachmentCount = 1;
			pipelineDesc.m_blendState.m_attachments[0] = overBlendAttachment;

			pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
			pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc, *renderPassDescription, renderPass);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[9] = {};

			// instance data
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = INSTANCE_DATA_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[0].pBufferInfo = &data.m_instanceDataBufferInfo;

			// transform data
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = TRANSFORM_DATA_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[1].pBufferInfo = &data.m_transformDataBufferInfo;

			// material data
			descriptorWrites[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[2].dstSet = descriptorSet;
			descriptorWrites[2].dstBinding = MATERIAL_DATA_BINDING;
			descriptorWrites[2].dstArrayElement = 0;
			descriptorWrites[2].descriptorCount = 1;
			descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[2].pBufferInfo = &data.m_materialDataBufferInfo;

			// shadow atlas
			VkDescriptorImageInfo shadowAtlasImageInfo = registry.getImageInfo(data.m_shadowAtlasImageHandle);
			shadowAtlasImageInfo.sampler = data.m_renderResources->m_shadowSampler;
			descriptorWrites[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[3].dstSet = descriptorSet;
			descriptorWrites[3].dstBinding = SHADOW_ATLAS_BINDING;
			descriptorWrites[3].dstArrayElement = 0;
			descriptorWrites[3].descriptorCount = 1;
			descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[3].pImageInfo = &shadowAtlasImageInfo;

			// directional light
			descriptorWrites[4] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[4].dstSet = descriptorSet;
			descriptorWrites[4].dstBinding = DIRECTIONAL_LIGHT_DATA_BINDING;
			descriptorWrites[4].dstArrayElement = 0;
			descriptorWrites[4].descriptorCount = 1;
			descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[4].pBufferInfo = &data.m_directionalLightDataBufferInfo;

			// point light
			descriptorWrites[5] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[5].dstSet = descriptorSet;
			descriptorWrites[5].dstBinding = POINT_LIGHT_DATA_BINDING;
			descriptorWrites[5].dstArrayElement = 0;
			descriptorWrites[5].descriptorCount = 1;
			descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[5].pBufferInfo = &data.m_pointLightDataBufferInfo;

			// shadow data
			descriptorWrites[6] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[6].dstSet = descriptorSet;
			descriptorWrites[6].dstBinding = SHADOW_DATA_BINDING;
			descriptorWrites[6].dstArrayElement = 0;
			descriptorWrites[6].descriptorCount = 1;
			descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[6].pBufferInfo = &data.m_shadowDataBufferInfo;

			// point light z bins
			descriptorWrites[7] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[7].dstSet = descriptorSet;
			descriptorWrites[7].dstBinding = POINT_LIGHT_Z_BINS_BINDING;
			descriptorWrites[7].dstArrayElement = 0;
			descriptorWrites[7].descriptorCount = 1;
			descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[7].pBufferInfo = &data.m_pointLightZBinsBufferInfo;

			// point light mask
			VkDescriptorBufferInfo pointLightMaskBufferInfo = registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle);
			descriptorWrites[8] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[8].dstSet = descriptorSet;
			descriptorWrites[8].dstBinding = POINT_LIGHT_MASK_BINDING;
			descriptorWrites[8].dstArrayElement = 0;
			descriptorWrites[8].descriptorCount = 1;
			descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[8].pBufferInfo = &pointLightMaskBufferInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
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

		vkCmdBindIndexBuffer(cmdBuf, data.m_renderResources->m_indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

		VkBuffer vertexBuffer = data.m_renderResources->m_vertexBuffer.getBuffer();
		VkBuffer vertexBuffers[] = { vertexBuffer, vertexBuffer, vertexBuffer };
		VkDeviceSize vertexBufferOffsets[] = { 0, RendererConsts::MAX_VERTICES * sizeof(VertexPosition), RendererConsts::MAX_VERTICES * (sizeof(VertexPosition) + sizeof(VertexNormal)) };

		vkCmdBindVertexBuffers(cmdBuf, 0, 3, vertexBuffers, vertexBufferOffsets);

		const glm::mat4 rowMajorInvViewMatrix = glm::transpose(glm::inverse(data.m_commonRenderData->m_viewMatrix));

		PushConsts pushConsts;
		pushConsts.jitteredViewProjectionMatrix = data.m_commonRenderData->m_jitteredViewProjectionMatrix;
		pushConsts.invViewMatrixRow0 = rowMajorInvViewMatrix[0];
		pushConsts.invViewMatrixRow1 = rowMajorInvViewMatrix[1];
		pushConsts.invViewMatrixRow2 = rowMajorInvViewMatrix[2];
		pushConsts.pointLightdirectionalLightCount = data.m_commonRenderData->m_pointLightCount << 16 | (data.m_commonRenderData->m_directionalLightCount & 0xFFFF);
		pushConsts.width = data.m_width;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConsts), &pushConsts);

		vkCmdDrawIndexedIndirect(cmdBuf, registry.getBuffer(data.m_indirectBufferHandle), 0, data.m_drawCount, sizeof(VkDrawIndexedIndirectCommand));
	});
}
