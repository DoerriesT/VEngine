#include "RasterTilingPass.h"
#include "Graphics/Vulkan/VKPipelineCache.h"
#include "Graphics/Vulkan/VKDescriptorSetCache.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKContext.h"
#include "Graphics/LightData.h"
#include <glm/gtx/transform.hpp>
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/Vulkan/RenderPassCache.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/DeferredObjectDeleter.h"
#include "Utility/Utility.h"
#include <glm/vec3.hpp>

namespace
{
	using namespace glm;
#include "../../../../../Application/Resources/Shaders/rasterTiling_bindings.h"
}

void VEngine::RasterTilingPass::addToGraph(RenderGraph &graph, const Data &data)
{
	ResourceUsageDescription passUsages[]
	{
		{ResourceViewHandle(data.m_pointLightBitMaskBufferHandle), ResourceState::WRITE_STORAGE_BUFFER_FRAGMENT_SHADER},
	};

	graph.addPass("Raster Tiling", QueueType::GRAPHICS, sizeof(passUsages) / sizeof(passUsages[0]), passUsages, [=](VkCommandBuffer cmdBuf, const Registry &registry)
	{
		const uint32_t width = data.m_passRecordContext->m_commonRenderData->m_width;
		const uint32_t height = data.m_passRecordContext->m_commonRenderData->m_height;

		// create pipeline description
		VkDynamicState dynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.setVertexShader("Resources/Shaders/rasterTiling_vert.spv");
		pipelineDesc.setFragmentShader("Resources/Shaders/rasterTiling_frag.spv");
		pipelineDesc.setVertexBindingDescription({ 0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX });
		pipelineDesc.setVertexAttributeDescription({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 });
		pipelineDesc.setPolygonModeCullMode(VK_POLYGON_MODE_FILL, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		pipelineDesc.setMultisampleState(VK_SAMPLE_COUNT_4_BIT, false, 0.0f, 0xFFFFFFFF, false, false);
		pipelineDesc.setDynamicState(sizeof(dynamicState) / sizeof(dynamicState[0]), dynamicState);
		pipelineDesc.finalize();

		// begin renderpass
		VkRenderPass renderPass;
		RenderPassCompatDesc renderPassCompatDesc;
		{
			RenderPassDesc renderPassDesc{};
			renderPassDesc.m_attachmentCount = 0;
			renderPassDesc.m_subpassCount = 1;
			renderPassDesc.m_subpasses[0] = { 0, 0, false, 0 };

			data.m_passRecordContext->m_renderPassCache->getRenderPass(renderPassDesc, renderPassCompatDesc, renderPass);

			VkFramebuffer framebuffer;
			
			VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferCreateInfo.renderPass = renderPass;
			framebufferCreateInfo.attachmentCount = renderPassDesc.m_attachmentCount;
			framebufferCreateInfo.pAttachments = nullptr;
			framebufferCreateInfo.width = width / 2;
			framebufferCreateInfo.height = height / 2;
			framebufferCreateInfo.layers = 1;

			if (vkCreateFramebuffer(g_context.m_device, &framebufferCreateInfo, nullptr, &framebuffer) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create framebuffer!", EXIT_FAILURE);
			}

			data.m_passRecordContext->m_deferredObjectDeleter->add(framebuffer);

			VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.framebuffer = framebuffer;
			renderPassBeginInfo.renderArea = { {}, {width / 2, height / 2} };
			renderPassBeginInfo.clearValueCount = 0;
			renderPassBeginInfo.pClearValues = nullptr;

			vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		}

		auto pipelineData = data.m_passRecordContext->m_pipelineCache->getPipeline(pipelineDesc, renderPassCompatDesc, 0, renderPass);

		VkDescriptorSet descriptorSet = data.m_passRecordContext->m_descriptorSetCache->getDescriptorSet(pipelineData.m_descriptorSetLayoutData.m_layouts[0], pipelineData.m_descriptorSetLayoutData.m_counts[0]);

		// update descriptor sets
		{
			VKDescriptorSetWriter writer(g_context.m_device, descriptorSet);

			writer.writeBufferInfo(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, registry.getBufferInfo(data.m_pointLightBitMaskBufferHandle), POINT_LIGHT_MASK_BINDING);

			writer.commit();
		}

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_layout, 0, 1, &descriptorSet, 0, nullptr);

		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(width / 2);
		viewport.height = static_cast<float>(height / 2);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = { width / 2, height / 2 };

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		vkCmdBindIndexBuffer(cmdBuf, data.m_passRecordContext->m_renderResources->m_lightProxyIndexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT16);

		VkBuffer vertexBuffer = data.m_passRecordContext->m_renderResources->m_lightProxyVertexBuffer.getBuffer();
		VkDeviceSize vertexOffset = 0;
		vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer, &vertexOffset);

		uint32_t alignedDomainSizeX = (width + RendererConsts::LIGHTING_TILE_SIZE - 1) / RendererConsts::LIGHTING_TILE_SIZE;
		uint32_t wordCount = static_cast<uint32_t>((data.m_lightData->m_pointLightData.size() + 31) / 32);

		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConsts, alignedDomainSizeX), sizeof(alignedDomainSizeX), &alignedDomainSizeX);
		vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, offsetof(PushConsts, wordCount), sizeof(wordCount), &wordCount);

		for (size_t i = 0; i < data.m_lightData->m_pointLightData.size(); ++i)
		{
			const auto &item = data.m_lightData->m_pointLightData[i];

			PushConsts pushConsts;
			pushConsts.transform = data.m_passRecordContext->m_commonRenderData->m_projectionMatrix * glm::translate(glm::vec3(item.m_positionRadius)) * glm::scale(glm::vec3(item.m_positionRadius.w));
			pushConsts.index = static_cast<uint32_t>(i);

			vkCmdPushConstants(cmdBuf, pipelineData.m_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, offsetof(PushConsts, alignedDomainSizeX), &pushConsts);

			vkCmdDrawIndexed(cmdBuf, 240, 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(cmdBuf);
	});
}
