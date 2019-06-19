#include "VKRasterTilingPass.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/LightData.h"
#include <glm/gtx/transform.hpp>

namespace
{
	using vec3 = glm::vec3;
	using vec4 = glm::vec4;
	using mat4 = glm::mat4;
	using uint = uint32_t;
#include "../../../../../Application/Resources/Shaders/rasterTiling_bindings.h"
}

void VEngine::VKRasterTilingPass::addToGraph(FrameGraph::Graph &graph, const Data &data)
{
	graph.addGraphicsPass("Tiling Pass", data.m_width / 2, data.m_height / 2,
		[&](FrameGraph::PassBuilder builder)
	{
		builder.writeStorageBuffer(data.m_pointLightBitMaskBufferHandle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	},
		[&](VkCommandBuffer cmdBuf, const FrameGraph::ResourceRegistry &registry, const VKRenderPassDescription *renderPassDescription, VkRenderPass renderPass)
	{
		// create pipeline description
		VKGraphicsPipelineDescription pipelineDesc;
		{
			strcpy_s(pipelineDesc.m_shaderStages.m_vertexShaderPath, "Resources/Shaders/rasterTiling_vert.spv");
			strcpy_s(pipelineDesc.m_shaderStages.m_fragmentShaderPath, "Resources/Shaders/rasterTiling_frag.spv");

			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 1;
			pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[0] = { 0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX };

			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 1;
			pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };

			pipelineDesc.m_inputAssemblyState.m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			pipelineDesc.m_inputAssemblyState.m_primitiveRestartEnable = false;

			pipelineDesc.m_viewportState.m_viewportCount = 1;
			pipelineDesc.m_viewportState.m_viewports[0] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
			pipelineDesc.m_viewportState.m_scissorCount = 1;
			pipelineDesc.m_viewportState.m_scissors[0] = { {0, 0}, {1, 1} };

			pipelineDesc.m_rasterizationState.m_depthClampEnable = false;
			pipelineDesc.m_rasterizationState.m_rasterizerDiscardEnable = false;
			pipelineDesc.m_rasterizationState.m_polygonMode = VK_POLYGON_MODE_FILL;
			pipelineDesc.m_rasterizationState.m_cullMode = VK_CULL_MODE_FRONT_BIT;
			pipelineDesc.m_rasterizationState.m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			pipelineDesc.m_rasterizationState.m_depthBiasEnable = false;
			pipelineDesc.m_rasterizationState.m_lineWidth = 1.0f;

			pipelineDesc.m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;
			pipelineDesc.m_multiSampleState.m_sampleShadingEnable = false;
			pipelineDesc.m_multiSampleState.m_sampleMask = 0xFFFFFFFF;

			pipelineDesc.m_depthStencilState.m_depthTestEnable = false;
			pipelineDesc.m_depthStencilState.m_depthWriteEnable = false;
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
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			// point light mask buffer
			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle), POINT_LIGHT_MASK_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(data.m_width / 2);
		viewport.height = static_cast<float>(data.m_height / 2);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = { data.m_width / 2, data.m_height / 2 };

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		vkCmdBindIndexBuffer(cmdBuf, data.m_renderResources->m_lightProxyIndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT16);

		VkBuffer vertexBuffer = data.m_renderResources->m_lightProxyVertexBuffer.getBuffer();
		VkDeviceSize vertexOffset = 0;
		vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer, &vertexOffset);

		uint32_t alignedDomainSizeX = (data.m_width / RendererConsts::LIGHTING_TILE_SIZE + ((data.m_width % RendererConsts::LIGHTING_TILE_SIZE == 0) ? 0 : 1));
		uint32_t wordCount = static_cast<uint32_t>((data.m_lightData->m_pointLightData.size() + 31) / 32);

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConsts, alignedDomainSizeX), sizeof(alignedDomainSizeX), &alignedDomainSizeX);
		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConsts, wordCount), sizeof(wordCount), &wordCount);

		for (size_t i = 0; i < data.m_lightData->m_pointLightData.size(); ++i)
		{
			const auto &item = data.m_lightData->m_pointLightData[i];

			PushConsts pushConsts;
			pushConsts.transform = data.m_projection * glm::translate(glm::vec3(item.m_positionRadius)) * glm::scale(glm::vec3(item.m_positionRadius.w));
			pushConsts.index = static_cast<uint32_t>(i);

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, offsetof(PushConsts, alignedDomainSizeX), &pushConsts);

			vkCmdDrawIndexed(cmdBuf, 240, 1, 0, 0, 0);
		}
	});
}
