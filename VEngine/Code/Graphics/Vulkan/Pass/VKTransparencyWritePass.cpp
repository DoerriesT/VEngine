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
			pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
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
			VkSampler pointSamplerClamp = data.m_renderResources->m_pointSamplerClamp;

			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// instance data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_instanceDataBufferInfo, INSTANCE_DATA_BINDING);

			// transform data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_transformDataBufferInfo, TRANSFORM_DATA_BINDING);

			// material data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING);

			// shadow atlas
			writer.writeImageInfo(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, registry.getImageInfo(data.m_shadowAtlasImageHandle, data.m_renderResources->m_shadowSampler), SHADOW_ATLAS_BINDING);

			// directional light
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_directionalLightDataBufferInfo, DIRECTIONAL_LIGHT_DATA_BINDING);

			// point light
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_pointLightDataBufferInfo, POINT_LIGHT_DATA_BINDING);

			// shadow data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_shadowDataBufferInfo, SHADOW_DATA_BINDING);

			// point light z bins
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_pointLightZBinsBufferInfo, POINT_LIGHT_Z_BINS_BINDING);

			// point light mask
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle), POINT_LIGHT_MASK_BINDING);

			// material data
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, data.m_materialDataBufferInfo, MATERIAL_DATA_BINDING);

			// shadow splits
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_shadowSplitsBufferHandle), SPLITS_BINDING);

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

		vkCmdBindIndexBuffer(cmdBuf, data.m_renderResources->m_indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT16);

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
