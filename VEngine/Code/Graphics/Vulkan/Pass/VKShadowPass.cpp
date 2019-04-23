#include "VKShadowPass.h"
#include "Graphics/LightData.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/Vulkan/VKUtility.h"
#include "Graphics/Mesh.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"

using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/shadows_bindings.h"

void VEngine::VKShadowPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addGraphicsPass("Shadow Pass", data.m_width, data.m_height,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.readStorageBuffer(data.m_transformDataBufferHandle, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

		builder.writeDepthStencil(data.m_shadowAtlasImageHandle);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKGraphicsPipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_shaderStages.m_vertexShaderPath, data.m_alphaMasked ? "Resources/Shaders/shadows_alpha_mask_vert.spv" : "Resources/Shaders/shadows_vert.spv");

			if (data.m_alphaMasked)
			{
				strcpy_s(pipelineDesc.m_shaderStages.m_fragmentShaderPath, "Resources/Shaders/shadows_frag.spv");
			}

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

			pipelineDesc.m_blendState.m_logicOpEnable = false;
			pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
			pipelineDesc.m_blendState.m_attachmentCount = 0;

			pipelineDesc.m_dynamicState.m_dynamicStateCount = 2;
			pipelineDesc.m_dynamicState.m_dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
			pipelineDesc.m_dynamicState.m_dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

			pipelineDesc.finalize();
		}

		auto pipelineData = data.m_pipelineCache->getPipeline(pipelineDesc, *renderPassDescription, renderPass);

		VkDescriptorSet descriptorSet = data.m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		if (data.m_alphaMasked)
		{
			VkWriteDescriptorSet descriptorWrites[2] = {};

			// transform data
			VkDescriptorBufferInfo transformBufferInfo = registry.getBufferInfo(data.m_transformDataBufferHandle);
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = TRANSFORM_DATA_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[0].pBufferInfo = &transformBufferInfo;

			// material data
			VkDescriptorBufferInfo materialBufferInfo = registry.getBufferInfo(data.m_materialDataBufferHandle);
			descriptorWrites[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[1].dstSet = descriptorSet;
			descriptorWrites[1].dstBinding = MATERIAL_DATA_BINDING;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[1].pBufferInfo = &materialBufferInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
		}
		else
		{
			VkWriteDescriptorSet descriptorWrites[1] = {};

			// transform data
			VkDescriptorBufferInfo bufferInfo = registry.getBufferInfo(data.m_transformDataBufferHandle);
			descriptorWrites[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			descriptorWrites[0].dstSet = descriptorSet;
			descriptorWrites[0].dstBinding = TRANSFORM_DATA_BINDING;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptorWrites[0].pBufferInfo = &bufferInfo;

			vkUpdateDescriptorSets(g_context.m_device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]), descriptorWrites, 0, nullptr);
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

		if (data.m_alphaMasked)
		{
			VkDescriptorSet descriptorSets[] = { descriptorSet , data.m_renderResources->m_textureDescriptorSet };
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 2, descriptorSets, 0, nullptr);
		}
		else
		{
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);
		}

		VkViewport viewport = { 0.0f, 0.0f, static_cast<float>(data.m_width), static_cast<float>(data.m_height), 0.0f, 1.0f };
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		// clear the relevant areas in the atlas
		if (data.m_clear)
		{
			VkClearRect clearRects[8];

			VkClearAttachment clearAttachment = {};
			clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			clearAttachment.clearValue.depthStencil = { 1.0f, 0 };

			for (size_t i = 0; i < data.m_shadowJobCount; ++i)
			{
				const ShadowJob &job = data.m_shadowJobs[i];

				clearRects[i].rect = { { int32_t(job.m_offsetX), int32_t(job.m_offsetY) },{ job.m_size, job.m_size } };
				clearRects[i].baseArrayLayer = 0;
				clearRects[i].layerCount = 1;
			}

			vkCmdClearAttachments(cmdBuf, 1, &clearAttachment, data.m_shadowJobCount, clearRects);
		}

		vkCmdBindIndexBuffer(cmdBuf, data.m_renderResources->m_indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

		for (size_t i = 0; i < data.m_shadowJobCount; ++i)
		{
			const ShadowJob &job = data.m_shadowJobs[i];

			// set viewport and scissor
			VkViewport viewport = { static_cast<float>(job.m_offsetX), static_cast<float>(job.m_offsetY), static_cast<float>(job.m_size), static_cast<float>(job.m_size), 0.0f, 1.0f };
			VkRect2D scissor = { { static_cast<int32_t>(job.m_offsetX), static_cast<int32_t>(job.m_offsetY) }, { job.m_size, job.m_size } };

			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
			vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | (data.m_alphaMasked ? VK_SHADER_STAGE_FRAGMENT_BIT : 0), 0, sizeof(glm::mat4), &job.m_shadowViewProjectionMatrix);

			VkBuffer vertexBuffer = data.m_renderResources->m_vertexBuffer.getBuffer();

			for (uint32_t i = 0; i < data.m_subMeshInstanceCount; ++i)
			{
				const SubMeshInstanceData &instance = data.m_subMeshInstances[i];
				const SubMeshData &subMesh = data.m_subMeshData[instance.m_subMeshIndex];

				vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer, &subMesh.m_vertexOffset);

				vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | (data.m_alphaMasked ? VK_SHADER_STAGE_FRAGMENT_BIT : 0), offsetof(PushConsts, transformIndex), sizeof(instance.m_transformIndex), &instance.m_transformIndex);
				
				if (data.m_alphaMasked)
				{
					vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConsts, materialIndex), sizeof(instance.m_materialIndex), &instance.m_materialIndex);
				}

				vkCmdDrawIndexed(cmdBuf, subMesh.m_indexCount, 1, subMesh.m_baseIndex, 0, 0);
			}
		}
	});
}
