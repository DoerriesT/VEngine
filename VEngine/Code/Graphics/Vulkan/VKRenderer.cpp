#include "VKRenderer.h"
#include "VKSwapChain.h"
#include "VKRenderResources.h"
#include "RenderPass/VKShadowRenderPass.h"
#include "RenderPass/VKGeometryRenderPass.h"
#include "RenderPass/VKForwardRenderPass.h"
#include "Pipeline/VKTilingPipeline.h"
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
	m_tilingPipeline.reset(new VKTilingPipeline());
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

	m_renderResources->createFramebuffer(width, height, m_geometryRenderPass->get(), m_shadowRenderPass->get(), m_forwardRenderPass->get());
	m_renderResources->createUniformBuffer(sizeof(RenderParams), sizeof(PerDrawData));
	m_renderResources->createStorageBuffers();
	m_renderResources->createCommandBuffers();
	m_renderResources->createDummyTexture();
	m_renderResources->createDescriptors();
	m_renderResources->createEvents();

	m_tilingPipeline->init(m_renderResources.get());
	m_geometryPipeline->init(width, height, m_geometryRenderPass->get(), m_renderResources.get());
	m_geometryAlphaMaskPipeline->init(width, height, m_geometryRenderPass->get(), m_renderResources.get());
	m_shadowPipeline->init(m_shadowRenderPass->get(), m_renderResources.get());
	m_lightingPipeline->init(m_renderResources.get());
	m_forwardPipeline->init(width, height, m_forwardRenderPass->get(), m_renderResources.get());

	m_geometryRenderPass->setPipelines(m_geometryPipeline.get(), m_geometryAlphaMaskPipeline.get());
	m_shadowRenderPass->setPipelines(m_shadowPipeline.get());
	m_forwardRenderPass->setPipelines(m_forwardPipeline.get());

}

void VEngine::VKRenderer::render(const RenderParams &renderParams, const DrawLists &drawLists, const LightData &lightData)
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

			size_t pointLightDataOffset = m_renderResources->m_directionalLightDataSize * MAX_DIRECTIONAL_LIGHTS;
			size_t spotLightDataOffset = pointLightDataOffset + m_renderResources->m_pointLightDataSize * MAX_POINT_LIGHTS;
			size_t shadowDataOffset = spotLightDataOffset + m_renderResources->m_spotLightDataSize * MAX_SPOT_LIGHTS;
			size_t pointCullDataOffset = shadowDataOffset + m_renderResources->m_shadowDataSize * MAX_SHADOW_DATA;
			size_t spotCullDataOffset = pointCullDataOffset + sizeof(glm::vec4) * MAX_POINT_LIGHTS + sizeof(glm::uvec4);

			// directional lights
			{
				for (const DirectionalLightData &item : lightData.m_directionalLightData)
				{
					memcpy(lightBuffer, &item, sizeof(item));
					lightBuffer += m_renderResources->m_directionalLightDataSize;
				}
			}
			
			// point lights
			{
				lightBuffer = ((unsigned char *)data) + pointLightDataOffset;
				for (const PointLightData &item : lightData.m_pointLightData)
				{
					memcpy(lightBuffer, &item, sizeof(item));
					lightBuffer += m_renderResources->m_pointLightDataSize;
				}
			}
			
			// spot lights
			{
				lightBuffer = ((unsigned char *)data) + spotLightDataOffset;
				for (const SpotLightData &item : lightData.m_spotLightData)
				{
					memcpy(lightBuffer, &item, sizeof(item));
					lightBuffer += m_renderResources->m_spotLightDataSize;
				}
			}
			
			// shadow data
			{
				lightBuffer = ((unsigned char *)data) + shadowDataOffset;
				for (const ShadowData &item : lightData.m_shadowData)
				{
					memcpy(lightBuffer, &item, sizeof(item));
					lightBuffer += m_renderResources->m_shadowDataSize;
				}
			}

			// point light cull data
			{
				lightBuffer = ((unsigned char *)data) + pointCullDataOffset;
				uint32_t count = lightData.m_pointLightData.size();
				memcpy(lightBuffer, &count, sizeof(count));
				lightBuffer += sizeof(glm::vec4);
				for (const PointLightData &item : lightData.m_pointLightData)
				{
					memcpy(lightBuffer, &item.m_positionRadius, sizeof(glm::vec4));
					lightBuffer += sizeof(glm::vec4);
				}
			}

			// spot light cull data
			{
				lightBuffer = ((unsigned char *)data) + spotCullDataOffset;
				uint32_t count = lightData.m_spotLightData.size();
				memcpy(lightBuffer, &count, sizeof(count));
				lightBuffer += sizeof(glm::uvec4);
				for (const SpotLightData &item : lightData.m_spotLightData)
				{
					memcpy(lightBuffer, &item.m_boundingSphere, sizeof(glm::vec4));
					lightBuffer += sizeof(glm::vec4);
				}
			}
		
			VkMappedMemoryRange directionalLightMemoryRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			directionalLightMemoryRange.memory = m_renderResources->m_lightDataStorageBuffer.m_memory;
			directionalLightMemoryRange.offset = 0;
			directionalLightMemoryRange.size = m_renderResources->m_directionalLightDataSize * lightData.m_directionalLightData.size();
			directionalLightMemoryRange.size = (directionalLightMemoryRange.size + g_context.m_properties.limits.nonCoherentAtomSize - 1) & ~(g_context.m_properties.limits.nonCoherentAtomSize - 1);

			VkMappedMemoryRange pointLightMemoryRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			pointLightMemoryRange.memory = m_renderResources->m_lightDataStorageBuffer.m_memory;
			pointLightMemoryRange.offset = pointLightDataOffset - (pointLightDataOffset % g_context.m_properties.limits.nonCoherentAtomSize);
			pointLightMemoryRange.size = m_renderResources->m_pointLightDataSize * lightData.m_pointLightData.size();
			pointLightMemoryRange.size = (pointLightMemoryRange.size + g_context.m_properties.limits.nonCoherentAtomSize - 1) & ~(g_context.m_properties.limits.nonCoherentAtomSize - 1);

			VkMappedMemoryRange spotLightMemoryRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			spotLightMemoryRange.memory = m_renderResources->m_lightDataStorageBuffer.m_memory;
			spotLightMemoryRange.offset = spotLightDataOffset - (spotLightDataOffset % g_context.m_properties.limits.nonCoherentAtomSize);
			spotLightMemoryRange.size = m_renderResources->m_spotLightDataSize * lightData.m_spotLightData.size();
			spotLightMemoryRange.size = (spotLightMemoryRange.size + g_context.m_properties.limits.nonCoherentAtomSize - 1) & ~(g_context.m_properties.limits.nonCoherentAtomSize - 1);

			VkMappedMemoryRange shadowDataMemoryRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			shadowDataMemoryRange.memory = m_renderResources->m_lightDataStorageBuffer.m_memory;
			shadowDataMemoryRange.offset = shadowDataOffset - (shadowDataOffset % g_context.m_properties.limits.nonCoherentAtomSize);
			shadowDataMemoryRange.size = m_renderResources->m_shadowDataSize * lightData.m_shadowData.size();
			shadowDataMemoryRange.size = (shadowDataMemoryRange.size + g_context.m_properties.limits.nonCoherentAtomSize - 1) & ~(g_context.m_properties.limits.nonCoherentAtomSize - 1);

			VkMappedMemoryRange pointLightCullDataMemoryRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			pointLightCullDataMemoryRange.memory = m_renderResources->m_lightDataStorageBuffer.m_memory;
			pointLightCullDataMemoryRange.offset = pointCullDataOffset - (pointCullDataOffset % g_context.m_properties.limits.nonCoherentAtomSize);
			pointLightCullDataMemoryRange.size = sizeof(glm::vec4) * lightData.m_pointLightData.size() + sizeof(glm::uvec4);
			pointLightCullDataMemoryRange.size = (pointLightCullDataMemoryRange.size + g_context.m_properties.limits.nonCoherentAtomSize - 1) & ~(g_context.m_properties.limits.nonCoherentAtomSize - 1);

			VkMappedMemoryRange spotLightCullDataMemoryRange = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
			spotLightCullDataMemoryRange.memory = m_renderResources->m_lightDataStorageBuffer.m_memory;
			spotLightCullDataMemoryRange.offset = spotCullDataOffset - (spotCullDataOffset % g_context.m_properties.limits.nonCoherentAtomSize);
			spotLightCullDataMemoryRange.size = sizeof(glm::vec4) * lightData.m_spotLightData.size() + sizeof(glm::uvec4);
			spotLightCullDataMemoryRange.size = (spotLightCullDataMemoryRange.size + g_context.m_properties.limits.nonCoherentAtomSize - 1) & ~(g_context.m_properties.limits.nonCoherentAtomSize - 1);

			VkMappedMemoryRange ranges[] = 
			{ 
				directionalLightMemoryRange, 
				pointLightMemoryRange,
				spotLightMemoryRange,
				shadowDataMemoryRange,
				pointLightCullDataMemoryRange,
				spotLightCullDataMemoryRange
			};

			vkFlushMappedMemoryRanges(g_context.m_device, sizeof(ranges) / sizeof(ranges[0]), ranges);
		}
	}

	// tiling
	{
		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(m_renderResources->m_tilingCommandBuffer, &beginInfo);
		{
			m_tilingPipeline->recordCommandBuffer(m_renderResources.get(), m_width, m_height);
		}
		vkEndCommandBuffer(m_renderResources->m_tilingCommandBuffer);

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_renderResources->m_tilingCommandBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		if (vkQueueSubmit(g_context.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to submit draw command buffer!", -1);
		}
	}

	m_geometryRenderPass->record(m_renderResources.get(), drawLists, m_width, m_height);
	m_geometryRenderPass->submit(m_renderResources.get());
	m_shadowRenderPass->record(m_renderResources.get(), drawLists, lightData, m_width, m_height);
	m_shadowRenderPass->submit(m_renderResources.get());

	// lighting
	{
		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(m_renderResources->m_lightingCommandBuffer, &beginInfo);
		{
			m_lightingPipeline->recordCommandBuffer(m_renderResources.get(), m_width, m_height);
		}
		vkEndCommandBuffer(m_renderResources->m_lightingCommandBuffer);

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_renderResources->m_lightingCommandBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;

		if (vkQueueSubmit(g_context.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to submit draw command buffer!", -1);
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

	m_forwardRenderPass->record(m_renderResources.get(), drawLists, m_swapChain->getImage(m_swapChainImageIndex), m_width, m_height);
	m_forwardRenderPass->submit(m_renderResources.get());

	VkSwapchainKHR swapChains[] = { m_swapChain->get() };

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &g_context.m_renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_swapChainImageIndex;

	result = vkQueuePresentKHR(g_context.m_presentQueue, &presentInfo);

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
