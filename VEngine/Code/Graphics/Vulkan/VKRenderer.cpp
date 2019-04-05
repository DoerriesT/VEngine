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
#include "Pass/VKLightingPass.h"
#include "Pass/VKBlitPass.h"
#include "Pass/VKMemoryHeapDebugPass.h"
#include "Pass/VKTextPass.h"
#include "Pass/VKRasterTilingPass.h"
#include "Pass/VKLuminanceHistogramPass.h"
#include "Pass/VKLuminanceHistogramReduceAveragePass.h"
#include "Pass/VKLuminanceHistogramDebugPass.h"
#include "Pass/VKTonemapPass.h"
#include "Pass/VKTAAResolvePass.h"
#include "Pass/VKVelocityCompositionPass.h"
#include "Pass/VKVelocityInitializationPass.h"
#include "VKPipelineManager.h"
#include <iostream>

VEngine::VKRenderer::VKRenderer(uint32_t width, uint32_t height, void *windowHandle)
{
	g_context.init(static_cast<GLFWwindow *>(windowHandle));

	m_renderResources = std::make_unique<VKRenderResources>();
	m_textureLoader = std::make_unique<VKTextureLoader>();
	m_swapChain = std::make_unique<VKSwapChain>(width, height);
	m_width = m_swapChain->getExtent().width;
	m_height = m_swapChain->getExtent().height;
	m_renderResources->init(m_width, m_height);

	m_fontAtlasTextureIndex = m_textureLoader->load("Resources/Textures/fontConsolas.dds");

	updateTextureData();

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_frameGraphs[i] = std::make_unique<FrameGraph::Graph>(*m_renderResources->m_syncPrimitiveAllocator, *m_renderResources->m_pipelineManager);
	}
}

VEngine::VKRenderer::~VKRenderer()
{
	vkDeviceWaitIdle(g_context.m_device);
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_frameGraphs[i].release();
	}
	m_swapChain.reset();
	m_textureLoader.reset();
	m_renderResources.reset();
}

void VEngine::VKRenderer::render(const RenderParams &renderParams, const DrawLists &drawLists, const LightData &lightData)
{
	RenderParams perFrameData = renderParams;
	perFrameData.m_currentResourceIndex = perFrameData.m_frame % RendererConsts::FRAMES_IN_FLIGHT;
	perFrameData.m_previousResourceIndex = (perFrameData.m_frame + RendererConsts::FRAMES_IN_FLIGHT - 1) % RendererConsts::FRAMES_IN_FLIGHT;

	FrameGraph::Graph &graph = *m_frameGraphs[perFrameData.m_currentResourceIndex];
	graph.reset();

	FrameGraph::ImageHandle swapchainTextureHandle = 0;

	FrameGraph::ImageHandle finalTextureHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "Final Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R8G8B8A8_UNORM;

		finalTextureHandle = graph.createImage(desc);
	}

	FrameGraph::ImageHandle shadowTextureHandle = 0;
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
			perFrameData.m_frame == 0 ? VK_NULL_HANDLE : m_renderResources->m_shadowTextureSemaphores[perFrameData.m_previousResourceIndex], // on first frame we dont wait
			m_renderResources->m_shadowTextureSemaphores[perFrameData.m_currentResourceIndex]);
	}

	FrameGraph::ImageHandle taaHistoryTextureHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "TAA History Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_taaHistoryTextures[perFrameData.m_previousResourceIndex].getFormat();

		taaHistoryTextureHandle = graph.importImage(desc,
			m_renderResources->m_taaHistoryTextures[perFrameData.m_previousResourceIndex].getImage(),
			m_renderResources->m_taaHistoryTextureViews[perFrameData.m_previousResourceIndex],
			&m_renderResources->m_taaHistoryTextureLayouts[perFrameData.m_previousResourceIndex],
			VK_NULL_HANDLE,
			VK_NULL_HANDLE);
	}

	FrameGraph::ImageHandle taaResolveTextureHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "TAA Resolve Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_taaHistoryTextures[perFrameData.m_currentResourceIndex].getFormat();

		taaResolveTextureHandle = graph.importImage(desc,
			m_renderResources->m_taaHistoryTextures[perFrameData.m_currentResourceIndex].getImage(),
			m_renderResources->m_taaHistoryTextureViews[perFrameData.m_currentResourceIndex],
			&m_renderResources->m_taaHistoryTextureLayouts[perFrameData.m_currentResourceIndex],
			VK_NULL_HANDLE,
			VK_NULL_HANDLE);
	}

	FrameGraph::ImageHandle depthTextureHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "DepthTexture";
		desc.m_concurrent = false;
		desc.m_clear = true;
		desc.m_clearValue.m_imageClearValue.depthStencil.depth = 1.0f;
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_D32_SFLOAT;

		depthTextureHandle = graph.createImage(desc);
	}

	FrameGraph::ImageHandle albedoTextureHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "AlbedoTexture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R8G8B8A8_UNORM;

		albedoTextureHandle = graph.createImage(desc);
	}

	FrameGraph::ImageHandle normalTextureHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "NormalTexture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		normalTextureHandle = graph.createImage(desc);
	}

	FrameGraph::ImageHandle materialTextureHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "MaterialTexture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R8G8B8A8_UNORM;

		materialTextureHandle = graph.createImage(desc);
	}

	FrameGraph::ImageHandle velocityTextureHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "VelocityTexture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16_SFLOAT;

		velocityTextureHandle = graph.createImage(desc);
	}

	FrameGraph::ImageHandle lightTextureHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "LightTexture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		lightTextureHandle = graph.createImage(desc);
	}

	FrameGraph::BufferHandle pointLightBitMaskBufferHandle = 0;
	{
		uint32_t w = m_width / RendererConsts::LIGHTING_TILE_SIZE + ((m_width % RendererConsts::LIGHTING_TILE_SIZE == 0) ? 0 : 1);
		uint32_t h = m_height / RendererConsts::LIGHTING_TILE_SIZE + ((m_height % RendererConsts::LIGHTING_TILE_SIZE == 0) ? 0 : 1);
		uint32_t tileCount = w * h;
		VkDeviceSize bufferSize = Utility::alignUp(lightData.m_pointLightData.size(), VkDeviceSize(32)) / 32 * sizeof(uint32_t) * tileCount;

		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Point Light Index Buffer";
		desc.m_concurrent = false;
		desc.m_clear = true;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = bufferSize;
		desc.m_size = desc.m_size < 4 ? 4 : desc.m_size;
		desc.m_hostVisible = false;

		pointLightBitMaskBufferHandle = graph.createBuffer(desc);
	}

	FrameGraph::BufferHandle perFrameDataBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Per Frame Data Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(RenderParams);
		desc.m_hostVisible = true;

		perFrameDataBufferHandle = graph.createBuffer(desc);
	}

	FrameGraph::BufferHandle perDrawDataBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Per Draw Data Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(PerDrawData) * (drawLists.m_opaqueItems.size() + drawLists.m_maskedItems.size());
		desc.m_size = desc.m_size < 4 ? 4 : desc.m_size;
		desc.m_hostVisible = true;

		perDrawDataBufferHandle = graph.createBuffer(desc);
	}

	FrameGraph::BufferHandle directionalLightDataBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "DirectionalLight Data Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(DirectionalLightData) * lightData.m_directionalLightData.size();
		desc.m_size = desc.m_size < 4 ? 4 : desc.m_size;
		desc.m_hostVisible = true;

		directionalLightDataBufferHandle = graph.createBuffer(desc);
	}

	FrameGraph::BufferHandle pointLightDataBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "PointLight Data Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(PointLightData) * lightData.m_pointLightData.size();
		desc.m_size = desc.m_size < 4 ? 4 : desc.m_size;
		desc.m_hostVisible = true;

		pointLightDataBufferHandle = graph.createBuffer(desc);
	}

	FrameGraph::BufferHandle shadowDataBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Shadow Data Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(ShadowData) * lightData.m_shadowData.size();
		desc.m_size = desc.m_size < 4 ? 4 : desc.m_size;
		desc.m_hostVisible = true;

		shadowDataBufferHandle = graph.createBuffer(desc);
	}

	FrameGraph::BufferHandle pointLightZBinsBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Z-Bin Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(uint32_t) * RendererConsts::Z_BINS;
		desc.m_hostVisible = true;

		pointLightZBinsBufferHandle = graph.createBuffer(desc);
	}

	FrameGraph::BufferHandle luminanceHistogramBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Luminance Histogram Buffer";
		desc.m_concurrent = false;
		desc.m_clear = true;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(uint32_t) * RendererConsts::LUMINANCE_HISTOGRAM_SIZE;
		desc.m_hostVisible = false;

		luminanceHistogramBufferHandle = graph.createBuffer(desc);
	}

	FrameGraph::BufferHandle avgLuminanceBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Average Luminance Buffer";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(float) * RendererConsts::FRAMES_IN_FLIGHT;
		desc.m_hostVisible = false;

		avgLuminanceBufferHandle = graph.importBuffer(desc, m_renderResources->m_avgLuminanceBuffer.getBuffer(), VK_NULL_HANDLE, VK_NULL_HANDLE);
	}


	// passes
	VKHostWritePass perFrameDataWritePass("Per Frame Data Write Pass", (unsigned char *)&perFrameData, 0, 0, sizeof(perFrameData), sizeof(perFrameData), sizeof(perFrameData), 1);

	VKHostWritePass perDrawDataWritePassOpaque("Per Draw Data Write Pass (Opaque)", (unsigned char *)drawLists.m_opaqueItems.data(),
		offsetof(DrawItem, m_perDrawData), 0, sizeof(PerDrawData), sizeof(PerDrawData), sizeof(DrawItem), drawLists.m_opaqueItems.size());

	VKHostWritePass perDrawDataWritePassMasked("Per Draw Data Write Pass (Masked)", (unsigned char *)drawLists.m_maskedItems.data(),
		offsetof(DrawItem, m_perDrawData), drawLists.m_opaqueItems.size() * sizeof(PerDrawData), sizeof(PerDrawData), sizeof(PerDrawData), sizeof(DrawItem), drawLists.m_maskedItems.size());

	VKHostWritePass directionalLightDataWritePass("Directional Light Data Write Pass", (unsigned char *)lightData.m_directionalLightData.data(),
		0, 0, lightData.m_directionalLightData.size() * sizeof(DirectionalLightData), lightData.m_directionalLightData.size() * sizeof(DirectionalLightData), lightData.m_directionalLightData.size(), 1);

	VKHostWritePass pointLightDataWritePass("Point Light Data Write Pass", (unsigned char *)lightData.m_pointLightData.data(),
		0, 0, lightData.m_pointLightData.size() * sizeof(PointLightData), lightData.m_pointLightData.size() * sizeof(PointLightData), lightData.m_pointLightData.size(), 1);

	VKHostWritePass spotLightDataWritePass("Spot Light Data Write Pass", (unsigned char *)lightData.m_spotLightData.data(),
		0, 0, lightData.m_spotLightData.size() * sizeof(SpotLightData), lightData.m_spotLightData.size() * sizeof(SpotLightData), lightData.m_spotLightData.size(), 1);

	VKHostWritePass shadowDataWritePass("Shadow Data Write Pass", (unsigned char *)lightData.m_shadowData.data(),
		0, 0, lightData.m_shadowData.size() * sizeof(ShadowData), lightData.m_shadowData.size() * sizeof(ShadowData), lightData.m_shadowData.size(), 1);

	VKHostWritePass pointLightZBinsWritePass("Point Light Z-Bins Write Pass", (unsigned char *)lightData.m_zBins.data(),
		0, 0, lightData.m_zBins.size() * sizeof(uint32_t), lightData.m_zBins.size() * sizeof(uint32_t), lightData.m_zBins.size(), 1);

	VKGeometryPass geometryPass(m_renderResources.get(), m_width, m_height, perFrameData.m_currentResourceIndex, drawLists.m_opaqueItems.size(), drawLists.m_opaqueItems.data(), 0, false);

	VKGeometryPass geometryAlphaMaskedPass(m_renderResources.get(), m_width, m_height, perFrameData.m_currentResourceIndex, drawLists.m_maskedItems.size(), drawLists.m_maskedItems.data(), drawLists.m_opaqueItems.size(), true);

	VKVelocityInitializationPass velocityInitializationPass(m_renderResources.get(), m_width, m_height, perFrameData.m_currentResourceIndex, perFrameData.m_prevViewProjectionMatrix * perFrameData.m_invViewProjectionMatrix);

	VKShadowPass shadowPass(m_renderResources.get(), g_shadowAtlasSize, g_shadowAtlasSize, perFrameData.m_currentResourceIndex, drawLists.m_opaqueItems.size(), drawLists.m_opaqueItems.data(), 0, lightData.m_shadowJobs.size(), lightData.m_shadowJobs.data());

	VKRasterTilingPass rasterTilingPass(m_renderResources.get(), m_width, m_height, perFrameData.m_currentResourceIndex, lightData, perFrameData.m_jitteredProjectionMatrix);

	VKLightingPass lightingPass(m_renderResources.get(), m_width, m_height, perFrameData.m_currentResourceIndex);

	VKTAAResolvePass taaResolvePass(m_renderResources.get(), m_width, m_height, perFrameData.m_currentResourceIndex);

	VKLuminanceHistogramPass luminanceHistogramPass(m_renderResources.get(), m_width, m_height, perFrameData.m_currentResourceIndex, -10.0f, 17.0f);

	VKLuminanceHistogramReduceAveragePass luminanceHistogramAveragePass(m_renderResources.get(), m_width, m_height, perFrameData.m_currentResourceIndex, perFrameData.m_timeDelta, -10.0f, 17.0f);

	VKTonemapPass tonemapPass(m_renderResources.get(), m_width, m_height, perFrameData.m_currentResourceIndex);

	VKMemoryHeapDebugPass memoryHeapDebugPass(m_width, m_height, 0.75f, 0.0f, 0.25f, 0.25f);

	VKLuminanceHistogramDebugPass luminanceHistogramDebugPass(m_renderResources.get(), m_width, m_height, perFrameData.m_currentResourceIndex, 0.5f, 0.0f, 0.5f, 1.0f);

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

	VKTextPass textPass(m_renderResources.get(), perFrameData.m_currentResourceIndex, m_width, m_height, m_fontAtlasTextureIndex, timingInfoCount, timingInfoStrings);

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

	// host write passes
	perFrameDataWritePass.addToGraph(graph, perFrameDataBufferHandle);
	if (!drawLists.m_opaqueItems.empty())
	{
		perDrawDataWritePassOpaque.addToGraph(graph, perDrawDataBufferHandle);
	}
	if (!drawLists.m_maskedItems.empty())
	{
		perDrawDataWritePassMasked.addToGraph(graph, perDrawDataBufferHandle);
	}
	if (!lightData.m_directionalLightData.empty())
	{
		directionalLightDataWritePass.addToGraph(graph, directionalLightDataBufferHandle);
	}
	if (!lightData.m_pointLightData.empty())
	{
		pointLightDataWritePass.addToGraph(graph, pointLightDataBufferHandle);
		pointLightZBinsWritePass.addToGraph(graph, pointLightZBinsBufferHandle);
	}
	if (!lightData.m_shadowData.empty())
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

	velocityInitializationPass.addToGraph(graph, depthTextureHandle, velocityTextureHandle);

	// draw shadows
	if (!lightData.m_shadowJobs.empty())
	{
		shadowPass.addToGraph(graph,
			perFrameDataBufferHandle,
			perDrawDataBufferHandle,
			shadowTextureHandle);
	}

	// cull lights to tiles
	if (!lightData.m_pointLightData.empty())
	{
		rasterTilingPass.addToGraph(graph, perFrameDataBufferHandle, pointLightBitMaskBufferHandle);
	}

	// light gbuffer
	{
		lightingPass.addToGraph(graph,
			perFrameDataBufferHandle,
			directionalLightDataBufferHandle,
			pointLightDataBufferHandle,
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

	// calculate luminance histograms
	luminanceHistogramPass.addToGraph(graph, lightTextureHandle, luminanceHistogramBufferHandle);

	// calculate avg luminance
	luminanceHistogramAveragePass.addToGraph(graph, luminanceHistogramBufferHandle, avgLuminanceBufferHandle);

	VkSwapchainKHR swapChain = m_swapChain->get();

	// get swapchain image
	{
		VkResult result = vkAcquireNextImageKHR(g_context.m_device, swapChain, std::numeric_limits<uint64_t>::max(), m_renderResources->m_swapChainImageAvailableSemaphores[perFrameData.m_currentResourceIndex], VK_NULL_HANDLE, &m_swapChainImageIndex);

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
			m_renderResources->m_swapChainImageAvailableSemaphores[perFrameData.m_currentResourceIndex],
			m_renderResources->m_swapChainRenderFinishedSemaphores[perFrameData.m_currentResourceIndex]);
	}

	// taa resolve
	if (g_TAAEnabled)
	{
		taaResolvePass.addToGraph(graph, perFrameDataBufferHandle, depthTextureHandle, velocityTextureHandle, taaHistoryTextureHandle, lightTextureHandle, taaResolveTextureHandle);

		lightTextureHandle = taaResolveTextureHandle;
	}

	// tonemap
	FrameGraph::ImageHandle tonemapTargetTextureHandle = m_swapChain->getImageFormat() != VK_FORMAT_R8G8B8A8_UNORM ? finalTextureHandle : swapchainTextureHandle;
	tonemapPass.addToGraph(graph, lightTextureHandle, tonemapTargetTextureHandle, avgLuminanceBufferHandle);

	//luminanceHistogramDebugPass.addToGraph(graph, perFrameDataBufferHandle, tonemapTargetTextureHandle, luminanceHistogramBufferHandle);
	//memoryHeapDebugPass.addToGraph(graph, tonemapTargetTextureHandle);
	//textPass.addToGraph(graph, tonemapTargetTextureHandle);

	// blit to swapchain image
	if (m_swapChain->getImageFormat() != VK_FORMAT_R8G8B8A8_UNORM)
	{
		blitPass.addToGraph(graph, tonemapTargetTextureHandle, swapchainTextureHandle);
	}

	graph.execute(FrameGraph::ResourceHandle(swapchainTextureHandle), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderResources->m_swapChainRenderFinishedSemaphores[perFrameData.m_currentResourceIndex];
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