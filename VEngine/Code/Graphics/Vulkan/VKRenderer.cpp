#include "VKRenderer.h"
#include "VKSwapChain.h"
#include "VKRenderResources.h"
#include "RenderPass/VKShadowRenderPass.h"
#include "RenderPass/VKGeometryRenderPass.h"
#include "RenderPass/VKForwardRenderPass.h"
#include "Pipeline/VKShadowPipeline.h"
#include "Pipeline/VKGeometryPipeline.h"
#include "Pipeline/VKGeometryAlphaMaskPipeline.h"
#include "Pipeline/VKLightingPipeline.h"
#include "Pipeline/VKForwardPipeline.h"
#include "Utility/Utility.h"
#include "VKUtility.h"
#include "Graphics/RenderParams.h"
#include "Graphics/DrawItem.h"
#include "Graphics/LightData.h"
#include "VKTextureLoader.h"
#include "GlobalVar.h"

VEngine::VKRenderer::VKRenderer()
	:m_width(),
	m_height(),
	m_swapChainImageIndex(),
	m_geometryRenderPass()
{
}

VEngine::VKRenderer::~VKRenderer()
{
}

void VEngine::VKRenderer::init(unsigned int width, unsigned int height)
{
	m_width = width;
	m_height = height;
	m_renderResources.reset(new VKRenderResources());
	m_textureLoader.reset(new VKTextureLoader());
	m_swapChain.reset(new VKSwapChain());
	m_shadowPipeline.reset(new VKShadowPipeline());
	m_geometryPipeline.reset(new VKGeometryPipeline());
	m_geometryAlphaMaskPipeline.reset(new VKGeometryAlphaMaskPipeline());
	m_lightingPipeline.reset(new VKLightingPipeline());
	m_forwardPipeline.reset(new VKForwardPipeline());

	m_renderResources->init(width, height);
	m_swapChain->init(width, height);

	m_geometryRenderPass.reset(new VKGeometryRenderPass(m_renderResources.get()));
	m_shadowRenderPass.reset(new VKShadowRenderPass(m_renderResources.get()));
	m_forwardRenderPass.reset(new VKForwardRenderPass(m_renderResources.get()));

	m_renderResources->createFramebuffer(width, height, m_geometryRenderPass->m_renderPass, m_shadowRenderPass->m_renderPass, m_forwardRenderPass->m_renderPass);
	m_renderResources->createUniformBuffer(sizeof(RenderParams), sizeof(PerDrawData));
	m_renderResources->createStorageBuffers();
	m_renderResources->createCommandBuffers();
	m_renderResources->createDummyTexture();
	m_renderResources->createDescriptors();
	m_renderResources->createEvents();

	m_geometryPipeline->init(width, height, m_geometryRenderPass->m_renderPass, m_renderResources.get());
	m_geometryAlphaMaskPipeline->init(width, height, m_geometryRenderPass->m_renderPass, m_renderResources.get());
	m_shadowPipeline->init(m_shadowRenderPass->m_renderPass, m_renderResources.get());
	m_lightingPipeline->init(m_renderResources.get());
	m_forwardPipeline->init(width, height, m_forwardRenderPass->m_renderPass, m_renderResources.get());

}

void VEngine::VKRenderer::update(const RenderParams &renderParams, const DrawLists &drawLists, const LightData &lightData)
{
	if (drawLists.m_opaqueItems.size() + drawLists.m_maskedItems.size() + drawLists.m_blendedItems.size() > MAX_UNIFORM_BUFFER_INSTANCE_COUNT)
	{
		Utility::fatalExit("Exceeded max DrawItem count!", -1);
	}

	// update UBO data
	{
		void *data = m_renderResources->m_mainUniformBuffer.m_mapped;

		// per frame data
		{
			RenderParams *perFrameData = (RenderParams *)data;
			*perFrameData = renderParams;
		}

		// per draw data
		{
			unsigned char *perDrawData = ((unsigned char *)data) + m_renderResources->m_perFrameDataSize;

			for (const DrawItem &item : drawLists.m_opaqueItems)
			{
				memcpy(perDrawData, &item.m_perDrawData, sizeof(item.m_perDrawData));
				perDrawData += m_renderResources->m_perDrawDataSize;
			}

			for (const DrawItem &item : drawLists.m_maskedItems)
			{
				memcpy(perDrawData, &item.m_perDrawData, sizeof(item.m_perDrawData));
				perDrawData += m_renderResources->m_perDrawDataSize;
			}

			for (const DrawItem &item : drawLists.m_blendedItems)
			{
				memcpy(perDrawData, &item.m_perDrawData, sizeof(item.m_perDrawData));
				perDrawData += m_renderResources->m_perDrawDataSize;
			}
		}

		VkMappedMemoryRange memoryRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		memoryRange.memory = m_renderResources->m_mainUniformBuffer.m_memory;
		memoryRange.offset = 0;
		memoryRange.size = m_renderResources->m_perFrameDataSize + (drawLists.m_opaqueItems.size() + drawLists.m_maskedItems.size() + drawLists.m_blendedItems.size()) * m_renderResources->m_perDrawDataSize;
		memoryRange.size = (memoryRange.size + g_context.m_properties.limits.nonCoherentAtomSize - 1) & ~(g_context.m_properties.limits.nonCoherentAtomSize - 1);

		vkFlushMappedMemoryRanges(g_context.m_device, 1, &memoryRange);
	}

	// update light data
	{
		void *data = m_renderResources->m_lightDataStorageBuffer.m_mapped;

		// directional lights
		{
			assert(lightData.m_directionalLightData.size() <= MAX_DIRECTIONAL_LIGHTS);
			assert(lightData.m_pointLightData.size() <= MAX_POINT_LIGHTS);
			assert(lightData.m_spotLightData.size() <= MAX_SPOT_LIGHTS);
			assert(lightData.m_shadowData.size() <= MAX_SHADOW_DATA);

			unsigned char *lightBuffer = ((unsigned char *)data);

			for (const DirectionalLightData &item : lightData.m_directionalLightData)
			{
				memcpy(lightBuffer, &item, sizeof(item));
				lightBuffer += m_renderResources->m_directionalLightDataSize;
			}



			//for (const PointLightData &item : lightData.m_pointLightData)
			//{
			//	memcpy(lightBuffer, &item, sizeof(item));
			//	lightBuffer += m_renderResources->m_pointLightDataSize;
			//}
			//
			//for (const SpotLightData &item : lightData.m_spotLightData)
			//{
			//	memcpy(lightBuffer, &item, sizeof(item));
			//	lightBuffer += m_renderResources->m_spotLightDataSize;
			//}
			//

			size_t shadowDataOffset = m_renderResources->m_directionalLightDataSize * MAX_DIRECTIONAL_LIGHTS
				+ m_renderResources->m_pointLightDataSize * MAX_POINT_LIGHTS
				+ m_renderResources->m_spotLightDataSize * MAX_SPOT_LIGHTS;
			lightBuffer = ((unsigned char *)data) + shadowDataOffset;

			for (const ShadowData &item : lightData.m_shadowData)
			{
				memcpy(lightBuffer, &item, sizeof(item));
				lightBuffer += m_renderResources->m_shadowDataSize;
			}

			VkMappedMemoryRange directionalLightMemoryRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			directionalLightMemoryRange.memory = m_renderResources->m_lightDataStorageBuffer.m_memory;
			directionalLightMemoryRange.offset = 0;
			directionalLightMemoryRange.size = m_renderResources->m_directionalLightDataSize * lightData.m_directionalLightData.size();
			directionalLightMemoryRange.size = (directionalLightMemoryRange.size + g_context.m_properties.limits.nonCoherentAtomSize - 1) & ~(g_context.m_properties.limits.nonCoherentAtomSize - 1);

			VkMappedMemoryRange shadowDataMemoryRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			shadowDataMemoryRange.memory = m_renderResources->m_lightDataStorageBuffer.m_memory;
			shadowDataMemoryRange.offset = shadowDataOffset - (shadowDataOffset % g_context.m_properties.limits.nonCoherentAtomSize);
			shadowDataMemoryRange.size = m_renderResources->m_shadowDataSize * lightData.m_shadowData.size();
			shadowDataMemoryRange.size = (shadowDataMemoryRange.size + g_context.m_properties.limits.nonCoherentAtomSize - 1) & ~(g_context.m_properties.limits.nonCoherentAtomSize - 1);

			VkMappedMemoryRange ranges[] = { directionalLightMemoryRange , shadowDataMemoryRange };

			vkFlushMappedMemoryRanges(g_context.m_device, sizeof(ranges) / sizeof(ranges[0]), ranges);
		}
	}

	VkResult result = vkAcquireNextImageKHR(g_context.m_device, m_swapChain->get(), std::numeric_limits<uint64_t>::max(), g_context.m_imageAvailableSemaphore, VK_NULL_HANDLE, &m_swapChainImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_swapChain->recreate(m_width, m_height);
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		Utility::fatalExit("Failed to acquire swap chain image!", -1);
	}

	// record commandbuffers
	{
		// geometry fill
		{
			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(m_renderResources->m_geometryFillCommandBuffer, &beginInfo);
			{
				VkClearValue clearValues[2] = {};
				clearValues[0].depthStencil = { 1.0f, 0 };
				clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };


				VkRenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.renderPass = m_geometryRenderPass->m_renderPass;
				renderPassInfo.framebuffer = m_renderResources->m_geometryFillFramebuffer;
				renderPassInfo.renderArea.offset = { 0, 0 };
				renderPassInfo.renderArea.extent = { m_width, m_height };
				renderPassInfo.clearValueCount = static_cast<uint32_t>(sizeof(clearValues) / sizeof(clearValues[0]));
				renderPassInfo.pClearValues = clearValues;

				vkCmdBeginRenderPass(m_renderResources->m_geometryFillCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				{
					// geometry pass
					m_geometryPipeline->recordCommandBuffer(m_geometryRenderPass->m_renderPass, m_renderResources.get(), drawLists);

					// geometry alpha mask pass
					vkCmdNextSubpass(m_renderResources->m_geometryFillCommandBuffer, VK_SUBPASS_CONTENTS_INLINE);
					m_geometryAlphaMaskPipeline->recordCommandBuffer(m_geometryRenderPass->m_renderPass, m_renderResources.get(), drawLists);
				}
				vkCmdEndRenderPass(m_renderResources->m_geometryFillCommandBuffer);
			}
			vkEndCommandBuffer(m_renderResources->m_geometryFillCommandBuffer);
		}

		// shadows
		{
			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(m_renderResources->m_shadowsCommandBuffer, &beginInfo);
			{
				VkRenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.renderPass = m_shadowRenderPass->m_renderPass;
				renderPassInfo.framebuffer = m_renderResources->m_shadowFramebuffer;
				renderPassInfo.renderArea.offset = { 0, 0 };
				renderPassInfo.renderArea.extent = { g_shadowAtlasSize, g_shadowAtlasSize };

				vkCmdBeginRenderPass(m_renderResources->m_shadowsCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				{
					m_shadowPipeline->recordCommandBuffer(m_shadowRenderPass->m_renderPass, m_renderResources.get(), drawLists, lightData);
				}
				vkCmdEndRenderPass(m_renderResources->m_shadowsCommandBuffer);

				// signal shadow event
				vkCmdSetEvent(m_renderResources->m_shadowsCommandBuffer, m_renderResources->m_shadowsFinishedEvent, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
			}
			vkEndCommandBuffer(m_renderResources->m_shadowsCommandBuffer);
		}

		// lighting
		{
			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(m_renderResources->m_lightingCommandBuffer, &beginInfo);
			{
				m_lightingPipeline->recordCommandBuffer(m_renderResources.get(), m_width, m_height);
			}
			vkEndCommandBuffer(m_renderResources->m_lightingCommandBuffer);
		}

		// forward / blit
		{
			VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(m_renderResources->m_forwardCommandBuffer, &beginInfo);
			{
				VkRenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.renderPass = m_forwardRenderPass->m_renderPass;
				renderPassInfo.framebuffer = m_renderResources->m_forwardFramebuffer;
				renderPassInfo.renderArea.offset = { 0, 0 };
				renderPassInfo.renderArea.extent = { m_width, m_height };

				vkCmdBeginRenderPass(m_renderResources->m_forwardCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				{
					m_forwardPipeline->recordCommandBuffer(m_forwardRenderPass->m_renderPass, m_renderResources.get(), drawLists);
				}
				vkCmdEndRenderPass(m_renderResources->m_forwardCommandBuffer);

				VkOffset3D blitSize;
				blitSize.x = m_width;
				blitSize.y = m_height;
				blitSize.z = 1;

				VkImageBlit imageBlitRegion = {};
				imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageBlitRegion.srcSubresource.layerCount = 1;
				imageBlitRegion.srcOffsets[1] = blitSize;
				imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageBlitRegion.dstSubresource.layerCount = 1;
				imageBlitRegion.dstOffsets[1] = blitSize;

				VkImageSubresourceRange subresourceRange = {};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.baseMipLevel = 0;
				subresourceRange.levelCount = 1;
				subresourceRange.baseArrayLayer = 0;
				subresourceRange.layerCount = 1;

				VKUtility::setImageLayout(
					m_renderResources->m_forwardCommandBuffer,
					m_swapChain->getImage(m_swapChainImageIndex),
					VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					subresourceRange,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT);

				vkCmdBlitImage(
					m_renderResources->m_forwardCommandBuffer,
					m_renderResources->m_lightAttachment.m_image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					m_swapChain->getImage(m_swapChainImageIndex),
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&imageBlitRegion,
					VK_FILTER_NEAREST);

				VKUtility::setImageLayout(
					m_renderResources->m_forwardCommandBuffer,
					m_swapChain->getImage(m_swapChainImageIndex),
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					subresourceRange,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
			}
			vkEndCommandBuffer(m_renderResources->m_forwardCommandBuffer);
		}
	}
}

void VEngine::VKRenderer::render()
{
	VkSemaphore waitSemaphores[] = { g_context.m_imageAvailableSemaphore };
	VkSemaphore signalSemaphores[] = { g_context.m_renderFinishedSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkCommandBuffer cmdBufs[] =
	{
		m_renderResources->m_geometryFillCommandBuffer,
		m_renderResources->m_shadowsCommandBuffer,
		m_renderResources->m_lightingCommandBuffer,
		m_renderResources->m_forwardCommandBuffer
	};

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = sizeof(cmdBufs) / sizeof(cmdBufs[0]);
	submitInfo.pCommandBuffers = cmdBufs;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(g_context.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to submit draw command buffer!", -1);
	}

	VkSwapchainKHR swapChains[] = { m_swapChain->get() };

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_swapChainImageIndex;

	VkResult result = vkQueuePresentKHR(g_context.m_presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		m_swapChain->recreate(m_width, m_height);
	}
	else if (result != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to present swap chain image!", -1);
	}

	vkQueueWaitIdle(g_context.m_presentQueue);
}

void VEngine::VKRenderer::reserveMeshBuffers(uint64_t vertexSize, uint64_t indexSize)
{
	m_renderResources->reserveMeshBuffers(vertexSize, indexSize);
}

void VEngine::VKRenderer::uploadMeshData(const unsigned char * vertices, uint64_t vertexSize, const unsigned char * indices, uint64_t indexSize)
{
	m_renderResources->uploadMeshData(vertices, vertexSize, indices, indexSize);
}

uint32_t VEngine::VKRenderer::loadTexture(const char *filepath)
{
	return m_textureLoader->load(filepath);
}

void VEngine::VKRenderer::freeTexture(uint32_t id)
{
	m_textureLoader->free(id);
}

void VEngine::VKRenderer::updateTextureData()
{
	m_renderResources->updateTextureArray(m_textureLoader->getTextures());
}
