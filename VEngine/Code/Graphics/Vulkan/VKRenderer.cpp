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
#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/Passes.h"

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
	{
		FrameGraph::Graph graph;

		FrameGraph::ImageDesc shadowDesc = {};
		shadowDesc.m_name = "ShadowAtlas";
		shadowDesc.m_initialState = FrameGraph::ImageInitialState::UNDEFINED;
		shadowDesc.m_width = g_shadowAtlasSize;
		shadowDesc.m_height = g_shadowAtlasSize;
		shadowDesc.m_format = VK_FORMAT_D16_UNORM;

		FrameGraph::ImageHandle depthHandle = 0;
		FrameGraph::ImageHandle albedoHandle = 0;
		FrameGraph::ImageHandle normalHandle = 0;
		FrameGraph::ImageHandle materialHandle = 0;
		FrameGraph::ImageHandle velocityHandle = 0;
		FrameGraph::ImageHandle lightHandle = 0;
		FrameGraph::ImageHandle shadowHandle = graph.createImage(shadowDesc);

		FrameGraph::BufferHandle tilingBufHandle = 0;

		FrameGraphPasses::addGBufferFillPass(graph,
			depthHandle,
			albedoHandle,
			normalHandle,
			materialHandle,
			velocityHandle);

		FrameGraphPasses::addGBufferFillAlphaPass(graph,
			depthHandle,
			albedoHandle,
			normalHandle,
			materialHandle,
			velocityHandle);

		FrameGraphPasses::addTilingPass(graph, tilingBufHandle);

		FrameGraphPasses::addShadowPass(graph, shadowHandle);

		FrameGraphPasses::addLightingPass(graph, 
			depthHandle, 
			albedoHandle, 
			normalHandle, 
			materialHandle, 
			shadowHandle, 
			tilingBufHandle, 
			lightHandle);

		FrameGraphPasses::addForwardPass(graph, 
			shadowHandle, 
			tilingBufHandle, 
			depthHandle, 
			velocityHandle, 
			lightHandle);

		graph.setBackBuffer(lightHandle);

		graph.compile();
	}



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
	m_renderResources->createDescriptors();
	m_renderResources->createEvents();

	updateTextureData();

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
		// per frame data
		{
			void *data;
			vmaMapMemory(g_context.m_allocator, m_renderResources->m_perFrameDataUniformBuffer.getAllocation(), &data);
			RenderParams *perFrameData = (RenderParams *)data;
			*perFrameData = renderParams;
			vmaUnmapMemory(g_context.m_allocator, m_renderResources->m_perFrameDataUniformBuffer.getAllocation());
		}

		// per draw data
		{
			void *data;
			vmaMapMemory(g_context.m_allocator, m_renderResources->m_perDrawDataUniformBuffer.getAllocation(), &data);
			unsigned char *perDrawData = ((unsigned char *)data);

			for (const DrawItem &item : drawLists.m_opaqueItems)
			{
				memcpy(perDrawData, &item.m_perDrawData, sizeof(item.m_perDrawData));
				perDrawData += sizeof(PerDrawData);
			}

			for (const DrawItem &item : drawLists.m_maskedItems)
			{
				memcpy(perDrawData, &item.m_perDrawData, sizeof(item.m_perDrawData));
				perDrawData += sizeof(PerDrawData);
			}

			for (const DrawItem &item : drawLists.m_blendedItems)
			{
				memcpy(perDrawData, &item.m_perDrawData, sizeof(item.m_perDrawData));
				perDrawData += sizeof(PerDrawData);
			}
			vmaUnmapMemory(g_context.m_allocator, m_renderResources->m_perDrawDataUniformBuffer.getAllocation());
		}
	}

	// update light data
	{
		assert(lightData.m_directionalLightData.size() <= MAX_DIRECTIONAL_LIGHTS);
		assert(lightData.m_pointLightData.size() <= MAX_POINT_LIGHTS);
		assert(lightData.m_spotLightData.size() <= MAX_SPOT_LIGHTS);
		assert(lightData.m_shadowData.size() <= MAX_SHADOW_DATA);

		// directional lights
		{
			void *data;
			vmaMapMemory(g_context.m_allocator, m_renderResources->m_directionalLightDataStorageBuffer.getAllocation(), &data);
			unsigned char *lightBuffer = ((unsigned char *)data);

			for (const DirectionalLightData &item : lightData.m_directionalLightData)
			{
				memcpy(lightBuffer, &item, sizeof(item));
				lightBuffer += sizeof(DirectionalLightData);
			}

			vmaUnmapMemory(g_context.m_allocator, m_renderResources->m_directionalLightDataStorageBuffer.getAllocation());
		}

		// point lights
		{
			void *data;
			vmaMapMemory(g_context.m_allocator, m_renderResources->m_pointLightDataStorageBuffer.getAllocation(), &data);
			unsigned char *lightBuffer = ((unsigned char *)data);

			for (const PointLightData &item : lightData.m_pointLightData)
			{
				memcpy(lightBuffer, &item, sizeof(item));
				lightBuffer += sizeof(PointLightData);
			}

			vmaUnmapMemory(g_context.m_allocator, m_renderResources->m_pointLightDataStorageBuffer.getAllocation());
		}

		// spot lights
		{
			void *data;
			vmaMapMemory(g_context.m_allocator, m_renderResources->m_spotLightDataStorageBuffer.getAllocation(), &data);
			unsigned char *lightBuffer = ((unsigned char *)data);

			for (const SpotLightData &item : lightData.m_spotLightData)
			{
				memcpy(lightBuffer, &item, sizeof(item));
				lightBuffer += sizeof(SpotLightData);
			}

			vmaUnmapMemory(g_context.m_allocator, m_renderResources->m_spotLightDataStorageBuffer.getAllocation());
		}

		// shadow data
		{
			void *data;
			vmaMapMemory(g_context.m_allocator, m_renderResources->m_shadowDataStorageBuffer.getAllocation(), &data);
			unsigned char *lightBuffer = ((unsigned char *)data);

			for (const ShadowData &item : lightData.m_shadowData)
			{
				memcpy(lightBuffer, &item, sizeof(item));
				lightBuffer += sizeof(ShadowData);
			}

			vmaUnmapMemory(g_context.m_allocator, m_renderResources->m_shadowDataStorageBuffer.getAllocation());
		}

		// zbins data
		{
			void *data;
			vmaMapMemory(g_context.m_allocator, m_renderResources->m_zBinStorageBuffer.getAllocation(), &data);
			memcpy(data, lightData.m_zBins.data(), lightData.m_zBins.size() * sizeof(glm::uvec2));
			vmaUnmapMemory(g_context.m_allocator, m_renderResources->m_zBinStorageBuffer.getAllocation());
		}

		// cull data
		{
			void *data;
			vmaMapMemory(g_context.m_allocator, m_renderResources->m_lightCullDataStorageBuffer.getAllocation(), &data);
			unsigned char *lightBuffer = ((unsigned char *)data);

			// count data
			{
				glm::uvec4 counts = glm::uvec4(lightData.m_pointLightData.size(), lightData.m_spotLightData.size(), 0, 0);
				memcpy(lightBuffer, &counts, sizeof(counts));
				lightBuffer += sizeof(glm::uvec4);
			}

			// point light cull data
			{
				for (const PointLightData &item : lightData.m_pointLightData)
				{
					memcpy(lightBuffer, &item.m_positionRadius, sizeof(glm::vec4));
					lightBuffer += sizeof(glm::vec4);
				}
			}

			// spot light cull data
			{
				for (const SpotLightData &item : lightData.m_spotLightData)
				{
					memcpy(lightBuffer, &item.m_boundingSphere, sizeof(glm::vec4));
					lightBuffer += sizeof(glm::vec4);
				}
			}

			vmaUnmapMemory(g_context.m_allocator, m_renderResources->m_lightCullDataStorageBuffer.getAllocation());
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

	result = vkQueuePresentKHR(g_context.m_graphicsQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		m_swapChain->recreate(m_width, m_height);
	}
	else if (result != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to present swap chain image!", -1);
	}

	vkQueueWaitIdle(g_context.m_graphicsQueue);
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
	const VkDescriptorImageInfo *imageInfos;
	size_t count;
	m_textureLoader->getDescriptorImageInfos(&imageInfos, count);
	m_renderResources->updateTextureArray(imageInfos, count);
}