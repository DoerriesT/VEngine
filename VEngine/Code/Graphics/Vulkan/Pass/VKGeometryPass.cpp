#include "VKGeometryPass.h"
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
#include "../../../../../Application/Resources/Shaders/geometry_bindings.h"
}

void VEngine::VKGeometryPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addGraphicsPass(data.m_alphaMasked ? "GBuffer Fill Alpha" : "GBuffer Fill", data.m_width, data.m_height,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.writeDepthStencil(data.m_depthImageHandle);
		builder.writeColorAttachment(data.m_albedoImageHandle, OUT_ALBEDO);
		builder.writeColorAttachment(data.m_normalImageHandle, OUT_NORMAL);
		builder.writeColorAttachment(data.m_metalnessRougnessOcclusionImageHandle, OUT_METALNESS_ROUGHNESS_OCCLUSION);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKGraphicsPipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/geometry_vert.spv");
			strcpy_s(pipelineDesc.m_shaderStages.m_fragmentShaderPath, data.m_alphaMasked ? "Resources/Shaders/geometry_alpha_mask_frag.spv" : "Resources/Shaders/geometry_frag.spv");

			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 1;
			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[0] = { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX };

			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 3;
			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) };
			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal), };
			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord), };

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

			VkPipelineColorBlendAttachmentState defaultBlendAttachment = {};
			defaultBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			defaultBlendAttachment.blendEnable = VK_FALSE;

			pipelineDesc.m_blendState.m_logicOpEnable = false;
			pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
			pipelineDesc.m_blendState.m_attachmentCount = 3; // 4
			pipelineDesc.m_blendState.m_attachments[0] = defaultBlendAttachment;
			pipelineDesc.m_blendState.m_attachments[1] = defaultBlendAttachment;
			pipelineDesc.m_blendState.m_attachments[2] = defaultBlendAttachment;
			//pipelineDesc.m_blendState.m_attachments[3] = defaultBlendAttachment;

			pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
			pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc, *renderPassDescription, renderPass);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VkWriteDescriptorSet descriptorWrites[2] = {};

			// transform data
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = TRANSFORM_DATA_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[0].pBufferInfo = &data.m_transformDataBufferInfo;

			// material data
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = MATERIAL_DATA_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[1].pBufferInfo = &data.m_materialDataBufferInfo;

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

		vkCmdBindIndexBuffer(cmdBuf, data.m_renderResources->m_meshBuffer.getBuffer(), RendererConsts::VERTEX_BUFFER_SIZE, VK_INDEX_TYPE_UINT32);

		VkBuffer vertexBuffer = data.m_renderResources->m_meshBuffer.getBuffer();
		VkDeviceSize vertexOffset = 0;
		vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer, &vertexOffset);

		const glm::mat4 rowMajorViewMatrix = glm::transpose(data.m_commonRenderData->m_viewMatrix);

		PushConsts pushConsts;
		pushConsts.jitteredViewProjectionMatrix = data.m_commonRenderData->m_jitteredViewProjectionMatrix;
		pushConsts.viewMatrixRow0 = rowMajorViewMatrix[0];
		pushConsts.viewMatrixRow1 = rowMajorViewMatrix[1];
		pushConsts.viewMatrixRow2 = rowMajorViewMatrix[2];

		const uint32_t perPassPushConstsSize = offsetof(PushConsts, transformIndex);
		const uint32_t perInstancePushConstsSize = sizeof(pushConsts) - perPassPushConstsSize;

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, perPassPushConstsSize, &pushConsts);

		for (uint32_t i = 0; i < data.m_subMeshInstanceCount; ++i)
		{
			const SubMeshInstanceData &instance = data.m_subMeshInstances[i];
			const SubMeshData &subMesh = data.m_subMeshData[instance.m_subMeshIndex];

			pushConsts.transformIndex = instance.m_transformIndex;
			pushConsts.materialIndex = instance.m_materialIndex;

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT, perPassPushConstsSize, perInstancePushConstsSize, ((uint8_t *)&pushConsts) + perPassPushConstsSize);

			vkCmdDrawIndexed(cmdBuf, subMesh.m_indexCount, 1, subMesh.m_baseIndex, subMesh.m_vertexOffset, 0);
		}
	});
}
