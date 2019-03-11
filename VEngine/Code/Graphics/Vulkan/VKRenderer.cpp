#include "VKRenderer.h"
#include "VKSwapChain.h"
#include "VKRenderResources.h"
#include "Utility/Utility.h"
#include "VKUtility.h"
#include "Graphics/RenderParams.h"
#include "Graphics/DrawItem.h"
#include "Graphics/LightData.h"
#include "VKTextureLoader.h"
#include "GlobalVar.h"
#include "FrameGraph/FrameGraph.h"
#include "Pass/VKHostWritePass.h"
#include "Pass/VKGeometryPass.h"
#include "Pass/VKShadowPass.h"
#include "Pass/VKTilingPass.h"
#include "Pass/VKLightingPass.h"
#include "Pass/VKBlitPass.h"
#include "Pass/VKMemoryHeapDebugPass.h"
#include "Pass/VKTextPass.h"
#include "VKPipelineManager.h"

VEngine::VKRenderer::VKRenderer()
	:m_width(),
	m_height(),
	m_swapChainImageIndex()
{
}

VEngine::VKRenderer::~VKRenderer()
{
}

void VEngine::VKRenderer::init(unsigned int width, unsigned int height)
{
	m_width = width;
	m_height = height;
	m_renderResources = std::make_unique<VKRenderResources>();
	m_textureLoader = std::make_unique<VKTextureLoader>();
	m_swapChain = std::make_unique<VKSwapChain>();
	m_swapChain->init(width, height);
	m_renderResources->init(width, height, VK_FORMAT_R16G16B16A16_SFLOAT);

	m_fontAtlasTextureIndex = m_textureLoader->load("Resources/Textures/fontConsolas.dds");

	updateTextureData();

	for (size_t i = 0; i < 2; ++i)
	{
		m_frameGraphs[i] = std::make_unique<FrameGraph::Graph>(*m_renderResources->m_syncPrimitiveAllocator, *m_renderResources->m_pipelineManager);
	}
}

void VEngine::VKRenderer::render(const RenderParams &renderParams, const DrawLists &drawLists, const LightData &lightData)
{
	if (drawLists.m_opaqueItems.size() + drawLists.m_maskedItems.size() + drawLists.m_blendedItems.size() > MAX_UNIFORM_BUFFER_INSTANCE_COUNT)
	{
		Utility::fatalExit("Exceeded max DrawItem count!", -1);
	}

	assert(lightData.m_directionalLightData.size() <= MAX_DIRECTIONAL_LIGHTS);
	assert(lightData.m_pointLightData.size() <= MAX_POINT_LIGHTS);
	assert(lightData.m_spotLightData.size() <= MAX_SPOT_LIGHTS);
	assert(lightData.m_shadowData.size() <= MAX_SHADOW_DATA);

	size_t frameIndex = renderParams.m_frame % FRAMES_IN_FLIGHT;

	FrameGraph::Graph &graph = *m_frameGraphs[frameIndex];
	graph.reset();

	size_t waitSemaphoreIndex = (renderParams.m_frame - 1) % FRAMES_IN_FLIGHT;
	size_t signalSemaphoreIndex = renderParams.m_frame % FRAMES_IN_FLIGHT;

	size_t alignedPerDrawDataSize = VKUtility::align(sizeof(PerDrawData), g_context.m_properties.limits.minUniformBufferOffsetAlignment);

	// buffer descriptions
	FrameGraph::BufferDescription perFrameDataBufferDesc = {};
	perFrameDataBufferDesc.m_name = "Per Frame Data Buffer";
	perFrameDataBufferDesc.m_concurrent = true;
	perFrameDataBufferDesc.m_clear = false;
	perFrameDataBufferDesc.m_clearValue.m_bufferClearValue = 0;
	perFrameDataBufferDesc.m_size = sizeof(RenderParams);
	perFrameDataBufferDesc.m_hostVisible = true;

	FrameGraph::BufferDescription perDrawDataBufferDesc = {};
	perDrawDataBufferDesc.m_name = "Per Draw Data Buffer";
	perDrawDataBufferDesc.m_concurrent = true;
	perDrawDataBufferDesc.m_clear = false;
	perDrawDataBufferDesc.m_clearValue.m_bufferClearValue = 0;
	perDrawDataBufferDesc.m_size = alignedPerDrawDataSize * MAX_UNIFORM_BUFFER_INSTANCE_COUNT;
	perDrawDataBufferDesc.m_hostVisible = true;

	FrameGraph::BufferDescription directionalLightDataBufferDesc = {};
	directionalLightDataBufferDesc.m_name = "DirectionalLight Data Buffer";
	directionalLightDataBufferDesc.m_concurrent = true;
	directionalLightDataBufferDesc.m_clear = false;
	directionalLightDataBufferDesc.m_clearValue.m_bufferClearValue = 0;
	directionalLightDataBufferDesc.m_size = sizeof(DirectionalLightData) * MAX_DIRECTIONAL_LIGHTS;
	directionalLightDataBufferDesc.m_hostVisible = true;

	FrameGraph::BufferDescription pointLightDataBufferDesc = {};
	pointLightDataBufferDesc.m_name = "PointLight Data Buffer";
	pointLightDataBufferDesc.m_concurrent = true;
	pointLightDataBufferDesc.m_clear = false;
	pointLightDataBufferDesc.m_clearValue.m_bufferClearValue = 0;
	pointLightDataBufferDesc.m_size = sizeof(PointLightData) * MAX_POINT_LIGHTS;
	pointLightDataBufferDesc.m_hostVisible = true;

	FrameGraph::BufferDescription spotLightDataBufferDesc = {};
	spotLightDataBufferDesc.m_name = "SpotLight Data Buffer";
	spotLightDataBufferDesc.m_concurrent = true;
	spotLightDataBufferDesc.m_clear = false;
	spotLightDataBufferDesc.m_clearValue.m_bufferClearValue = 0;
	spotLightDataBufferDesc.m_size = sizeof(SpotLightData) * MAX_SPOT_LIGHTS;
	spotLightDataBufferDesc.m_hostVisible = true;

	FrameGraph::BufferDescription shadowDataBufferDesc = {};
	shadowDataBufferDesc.m_name = "Shadow Data Buffer";
	shadowDataBufferDesc.m_concurrent = true;
	shadowDataBufferDesc.m_clear = false;
	shadowDataBufferDesc.m_clearValue.m_bufferClearValue = 0;
	shadowDataBufferDesc.m_size = sizeof(ShadowData) * MAX_SHADOW_DATA;
	shadowDataBufferDesc.m_hostVisible = true;

	FrameGraph::BufferDescription pointLightZBinsBufferDesc = {};
	pointLightZBinsBufferDesc.m_name = "Z-Bin Buffer";
	pointLightZBinsBufferDesc.m_concurrent = true;
	pointLightZBinsBufferDesc.m_clear = false;
	pointLightZBinsBufferDesc.m_clearValue.m_bufferClearValue = 0;
	pointLightZBinsBufferDesc.m_size = sizeof(uint32_t) * Z_BINS;
	pointLightZBinsBufferDesc.m_hostVisible = true;

	FrameGraph::BufferDescription pointLightCullDataBufferDesc = {};
	pointLightCullDataBufferDesc.m_name = "Light Cull Data Buffer";
	pointLightCullDataBufferDesc.m_concurrent = true;
	pointLightCullDataBufferDesc.m_clear = false;
	pointLightCullDataBufferDesc.m_clearValue.m_bufferClearValue = 0;
	pointLightCullDataBufferDesc.m_size = MAX_POINT_LIGHTS * sizeof(glm::vec4);
	pointLightCullDataBufferDesc.m_hostVisible = true;
	

	// passes
	VKHostWritePass perFrameDataWritePass(perFrameDataBufferDesc, "Per Frame Data Write Pass", (unsigned char *)&renderParams, 
		0, 0, sizeof(renderParams), sizeof(renderParams), sizeof(renderParams), 1);

	VKHostWritePass perDrawDataWritePassOpaque(perDrawDataBufferDesc, "Per Draw Data Write Pass (Opaque)", (unsigned char *)drawLists.m_opaqueItems.data(), 
		offsetof(DrawItem, m_perDrawData), 0, sizeof(PerDrawData), alignedPerDrawDataSize, sizeof(DrawItem), drawLists.m_opaqueItems.size());

	VKHostWritePass perDrawDataWritePassMasked(perDrawDataBufferDesc, "Per Draw Data Write Pass (Masked)", (unsigned char *)drawLists.m_maskedItems.data(),
		offsetof(DrawItem, m_perDrawData), drawLists.m_opaqueItems.size() * alignedPerDrawDataSize, sizeof(PerDrawData), alignedPerDrawDataSize, sizeof(DrawItem), drawLists.m_maskedItems.size());
	
	VKHostWritePass perDrawDataWritePassBlended(perDrawDataBufferDesc, "Per Draw Data Write Pass (Blended)", (unsigned char *)drawLists.m_blendedItems.data(),
		offsetof(DrawItem, m_perDrawData), (drawLists.m_opaqueItems.size() + drawLists.m_maskedItems.size()) * alignedPerDrawDataSize, sizeof(PerDrawData), alignedPerDrawDataSize, sizeof(DrawItem), drawLists.m_blendedItems.size());

	VKHostWritePass directionalLightDataWritePass(directionalLightDataBufferDesc, "Directional Light Data Write Pass", (unsigned char *)lightData.m_directionalLightData.data(),
		0, 0, lightData.m_directionalLightData.size() * sizeof(DirectionalLightData), lightData.m_directionalLightData.size() * sizeof(DirectionalLightData), lightData.m_directionalLightData.size(), 1);

	VKHostWritePass pointLightDataWritePass(pointLightDataBufferDesc, "Point Light Data Write Pass", (unsigned char *)lightData.m_pointLightData.data(),
		0, 0, lightData.m_pointLightData.size() * sizeof(PointLightData), lightData.m_pointLightData.size() * sizeof(PointLightData), lightData.m_pointLightData.size(), 1);

	VKHostWritePass spotLightDataWritePass(spotLightDataBufferDesc, "Spot Light Data Write Pass", (unsigned char *)lightData.m_spotLightData.data(),
		0, 0, lightData.m_spotLightData.size() * sizeof(SpotLightData), lightData.m_spotLightData.size() * sizeof(SpotLightData), lightData.m_spotLightData.size(), 1);

	VKHostWritePass shadowDataWritePass(shadowDataBufferDesc, "Shadow Data Write Pass", (unsigned char *)lightData.m_shadowData.data(),
		0, 0, lightData.m_shadowData.size() * sizeof(ShadowData), lightData.m_shadowData.size() * sizeof(ShadowData), lightData.m_shadowData.size(), 1);

	VKHostWritePass pointLightZBinsWritePass(pointLightZBinsBufferDesc, "Point Light Z-Bins Write Pass", (unsigned char *)lightData.m_zBins.data(),
		0, 0, lightData.m_zBins.size() * sizeof(uint32_t), lightData.m_zBins.size() * sizeof(uint32_t), lightData.m_zBins.size(), 1);

	VKHostWritePass pointLightCullDataWritePass(pointLightCullDataBufferDesc, "Point Light Cull Data Write Pass", (unsigned char *)lightData.m_pointLightData.data(),
		offsetof(PointLightData, m_positionRadius), 0, sizeof(glm::vec4), sizeof(glm::vec4), sizeof(PointLightData), lightData.m_pointLightData.size());

	VKGeometryPass geometryPass(m_renderResources.get(), m_width, m_height, frameIndex, drawLists.m_opaqueItems.size(), drawLists.m_opaqueItems.data(), 0, false);

	VKGeometryPass geometryAlphaMaskedPass(m_renderResources.get(), m_width, m_height, frameIndex, drawLists.m_maskedItems.size(), drawLists.m_maskedItems.data(), drawLists.m_opaqueItems.size(), true);

	VKShadowPass shadowPass(m_renderResources.get(), g_shadowAtlasSize, g_shadowAtlasSize, frameIndex, drawLists.m_opaqueItems.size(), drawLists.m_opaqueItems.data(), 0, lightData.m_shadowJobs.size(), lightData.m_shadowJobs.data());

	VKTilingPass tilingPass(m_renderResources.get(), m_width, m_height, frameIndex, lightData.m_pointLightData.size());

	VKLightingPass lightingPass(m_renderResources.get(), m_width, m_height, frameIndex);

	VKMemoryHeapDebugPass memoryHeapDebugPass(m_width, m_height, 0.0f, 0.0f, 1.0f, 1.0f);

	FrameGraph::PassTimingInfo timingInfos[128];
	size_t timingInfoCount;

	graph.getTimingInfo(timingInfoCount, timingInfos);

	VKTextPass::String timingInfoStrings[128];
	std::string timingInfoStringData[128];

	float totalPassOnly = 0.0f;
	float totalSyncOnly = 0.0f;
	float total = 0.0f;

	for (size_t i = 0; i < timingInfoCount; ++i)
	{
		timingInfoStringData[i] = std::to_string(timingInfos[i].m_passTimeWithSync) + "ms Pass+Sync   "
			+ std::to_string(timingInfos[i].m_passTime) + "ms Pass-Only   " 
			+ std::to_string(timingInfos[i].m_passTimeWithSync - timingInfos[i].m_passTime) + "ms Sync Only   "
			+ std::to_string(((timingInfos[i].m_passTimeWithSync - timingInfos[i].m_passTime) / timingInfos[i].m_passTimeWithSync) * 100.0f) + "% Sync of total Pass Time   "
			+ timingInfos[i].m_passName;
		timingInfoStrings[i].m_chars = timingInfoStringData[i].c_str();
		timingInfoStrings[i].m_positionX = 0;
		timingInfoStrings[i].m_positionY = i * 20;

		totalPassOnly += timingInfos[i].m_passTime;
		totalSyncOnly += timingInfos[i].m_passTimeWithSync - timingInfos[i].m_passTime;
		total += timingInfos[i].m_passTimeWithSync;
	}

	timingInfoStringData[timingInfoCount] = std::to_string(totalPassOnly) + "ms Total Pass-Only";
	timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	timingInfoStrings[timingInfoCount].m_positionX = 0;
	timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	++timingInfoCount;
	timingInfoStringData[timingInfoCount] = std::to_string(totalSyncOnly) + "ms Total Sync-Only";
	timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	timingInfoStrings[timingInfoCount].m_positionX = 0;
	timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	++timingInfoCount;
	timingInfoStringData[timingInfoCount] = std::to_string(total) + "ms Total";
	timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	timingInfoStrings[timingInfoCount].m_positionX = 0;
	timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	++timingInfoCount;
	timingInfoStringData[timingInfoCount] = std::to_string(totalSyncOnly / total * 100.0f) + "% Sync of total time";
	timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	timingInfoStrings[timingInfoCount].m_positionX = 0;
	timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	++timingInfoCount;

	VKTextPass textPass(m_renderResources.get(), m_width, m_height, m_fontAtlasTextureIndex, timingInfoCount, timingInfoStrings);

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

	VKBlitPass blitPass("Blit Pass", 1, &imageBlitRegion, VK_FILTER_NEAREST);

	FrameGraph::BufferHandle perFrameDataBufferHandle = 0;
	FrameGraph::BufferHandle perDrawDataBufferHandle = 0;
	FrameGraph::BufferHandle directionalLightDataBufferHandle = 0;
	FrameGraph::BufferHandle pointLightDataBufferHandle = 0;
	FrameGraph::BufferHandle spotLightDataBufferHandle = 0;
	FrameGraph::BufferHandle shadowDataBufferHandle = 0;
	FrameGraph::BufferHandle pointLightZBinsBufferHandle = 0;
	FrameGraph::BufferHandle pointLightCullDataBufferHandle = 0;
	FrameGraph::BufferHandle pointLightBitMaskBufferHandle = 0;
	FrameGraph::ImageHandle depthTextureHandle = 0;
	FrameGraph::ImageHandle albedoTextureHandle = 0;
	FrameGraph::ImageHandle normalTextureHandle = 0;
	FrameGraph::ImageHandle materialTextureHandle = 0;
	FrameGraph::ImageHandle velocityTextureHandle = 0;
	FrameGraph::ImageHandle lightTextureHandle = 0;
	FrameGraph::ImageHandle shadowTextureHandle = 0;
	FrameGraph::ImageHandle swapchainTextureHandle = 0;

	// import resources
	{
		// shadow atlas
		{
			FrameGraph::ImageDescription desc = {};
			desc.m_name = "Shadow Atlas";
			desc.m_concurrent = false;
			desc.m_clear = false;
			desc.m_clearValue.m_imageClearValue = {};
			desc.m_width = g_shadowAtlasSize;
			desc.m_height = g_shadowAtlasSize;
			desc.m_format = m_renderResources->m_shadowTexture.getFormat();


			shadowTextureHandle = graph.importImage(desc,
				m_renderResources->m_shadowTexture.getImage(),
				m_renderResources->m_shadowTextureView,
				&m_renderResources->m_shadowTextureLayout,
				renderParams.m_frame == 0 ? VK_NULL_HANDLE : m_renderResources->m_shadowTextureSemaphores[waitSemaphoreIndex], // on first frame we dont wait
				m_renderResources->m_shadowTextureSemaphores[signalSemaphoreIndex]);
		}
	}

	// host write passes
	perFrameDataWritePass.addToGraph(graph, perFrameDataBufferHandle);
	//if (!drawLists.m_opaqueItems.empty())
	{
		perDrawDataWritePassOpaque.addToGraph(graph, perDrawDataBufferHandle);
	}
	//if (!drawLists.m_maskedItems.empty())
	{
		perDrawDataWritePassMasked.addToGraph(graph, perDrawDataBufferHandle);
	}
	//if (!lightData.m_directionalLightData.empty())
	{
		directionalLightDataWritePass.addToGraph(graph, directionalLightDataBufferHandle);
	}
	//if (!lightData.m_pointLightData.empty())
	{
		pointLightDataWritePass.addToGraph(graph, pointLightDataBufferHandle);
		pointLightZBinsWritePass.addToGraph(graph, pointLightZBinsBufferHandle);
		pointLightCullDataWritePass.addToGraph(graph, pointLightCullDataBufferHandle);
	}
	//if (!lightData.m_spotLightData.empty())
	{
		spotLightDataWritePass.addToGraph(graph, spotLightDataBufferHandle);
	}
	//if (!lightData.m_shadowData.empty())
	{
		shadowDataWritePass.addToGraph(graph, shadowDataBufferHandle);
	}

	// draw opaque geometry to gbuffer
	if (!drawLists.m_opaqueItems.empty())
	{
		geometryPass.addToGraph(graph,
			perFrameDataBufferHandle,
			perDrawDataBufferHandle,
			depthTextureHandle,
			albedoTextureHandle,
			normalTextureHandle,
			materialTextureHandle,
			velocityTextureHandle);
	}

	// draw opaque alpha masked geometry to gbuffer
	if (!drawLists.m_maskedItems.empty())
	{
		geometryAlphaMaskedPass.addToGraph(graph,
			perFrameDataBufferHandle,
			perDrawDataBufferHandle,
			depthTextureHandle,
			albedoTextureHandle,
			normalTextureHandle,
			materialTextureHandle,
			velocityTextureHandle);
	}

	// draw shadows
	if (!lightData.m_shadowJobs.empty())
	{
		shadowPass.addToGraph(graph,
			perFrameDataBufferHandle,
			perDrawDataBufferHandle,
			shadowTextureHandle);
	}

	// cull lights to tiles
	{
		tilingPass.addToGraph(graph,
			perFrameDataBufferHandle,
			pointLightCullDataBufferHandle,
			pointLightBitMaskBufferHandle);
	}

	// light gbuffer
	{
		lightingPass.addToGraph(graph,
			perFrameDataBufferHandle,
			directionalLightDataBufferHandle,
			pointLightDataBufferHandle,
			spotLightDataBufferHandle,
			shadowDataBufferHandle,
			pointLightZBinsBufferHandle,
			pointLightBitMaskBufferHandle,
			depthTextureHandle,
			albedoTextureHandle,
			normalTextureHandle,
			materialTextureHandle,
			shadowTextureHandle,
			lightTextureHandle);
	}

	//memoryHeapDebugPass.addToGraph(graph, lightTextureHandle);
	textPass.addToGraph(graph, lightTextureHandle);

	// draw blended items
	//if (!drawLists.m_blendedItems.empty())
	//{
	//	m_forwardPipeline->addPass(graph,
	//		perFrameDataBufferHandle,
	//		perDrawDataBufferHandle,
	//		tiledLightBufferHandle,
	//		shadowTextureHandle,
	//		depthTextureHandle,
	//		lightTextureHandle,
	//		velocityTextureHandle,
	//		m_renderResources.get(),
	//		drawLists);
	//}

	// get swapchain image
	{
		VkResult result = vkAcquireNextImageKHR(g_context.m_device, m_swapChain->get(), std::numeric_limits<uint64_t>::max(), m_renderResources->m_swapChainImageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &m_swapChainImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_swapChain->recreate(m_width, m_height);
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			Utility::fatalExit("Failed to acquire swap chain image!", -1);
		}

		FrameGraph::ImageDescription desc = {};
		desc.m_name = "Swapchain Image";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_swapChain->getExtent().width;
		desc.m_height = m_swapChain->getExtent().height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = m_swapChain->getImageFormat();

		swapchainTextureHandle = graph.importImage(desc,
			m_swapChain->getImage(m_swapChainImageIndex),
			m_swapChain->getImageView(m_swapChainImageIndex),
			&m_renderResources->m_swapChainImageLayouts[m_swapChainImageIndex],
			m_renderResources->m_swapChainImageAvailableSemaphores[frameIndex],
			m_renderResources->m_swapChainRenderFinishedSemaphores[frameIndex]);
	}


	// blit to swapchain image
	{
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

		blitPass.addToGraph(graph, lightTextureHandle, swapchainTextureHandle);
	}

	//m_testPipeline->addPass(graph,
	//	perFrameDataBufferHandle,
	//	perDrawDataBufferHandle,
	//	depthTextureHandle,
	//	swapchainTextureHandle,
	//	frameIndex,
	//	m_renderResources.get(),
	//	drawLists);

	graph.execute(FrameGraph::ResourceHandle(swapchainTextureHandle));

	VkCommandBuffer cmdBuf = VKUtility::beginSingleTimeCommands(g_context.m_graphicsCommandPool);
	{
		VkImageMemoryBarrier imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier.dstAccessMask = 0;
		imageBarrier.oldLayout = m_renderResources->m_swapChainImageLayouts[m_swapChainImageIndex];
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.image = m_swapChain->getImage(m_swapChainImageIndex);
		imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1 };

		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

		m_renderResources->m_swapChainImageLayouts[m_swapChainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	VKUtility::endSingleTimeCommands(g_context.m_graphicsQueue, g_context.m_graphicsCommandPool, cmdBuf);

	VkSwapchainKHR swapChain = m_swapChain->get();

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderResources->m_swapChainRenderFinishedSemaphores[frameIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &m_swapChainImageIndex;


	if (vkQueuePresentKHR(g_context.m_graphicsQueue, &presentInfo) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to present!", -1);
	}
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