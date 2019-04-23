#include "VKRenderer.h"
#include "VKSwapChain.h"
#include "VKRenderResources.h"
#include "Utility/Utility.h"
#include "VKUtility.h"
#include "Graphics/RenderData.h"
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
#include "Pass/VKVelocityInitializationPass.h"
#include "VKPipelineCache.h"
#include "VKDescriptorSetCache.h"
#include "VKMaterialManager.h"
#include "VKResourceDefinitions.h"
#include <iostream>

VEngine::VKRenderer::VKRenderer(uint32_t width, uint32_t height, void *windowHandle)
{
	g_context.init(static_cast<GLFWwindow *>(windowHandle));

	m_pipelineCache = std::make_unique<VKPipelineCache>();
	m_descriptorSetCache = std::make_unique<VKDescriptorSetCache>();
	m_renderResources = std::make_unique<VKRenderResources>();
	m_textureLoader = std::make_unique<VKTextureLoader>(m_renderResources->m_stagingBuffer);
	m_materialManager = std::make_unique<VKMaterialManager>(m_renderResources->m_stagingBuffer, m_renderResources->m_materialBuffer);
	m_swapChain = std::make_unique<VKSwapChain>(width, height);
	m_width = m_swapChain->getExtent().width;
	m_height = m_swapChain->getExtent().height;
	m_renderResources->init(m_width, m_height);

	m_fontAtlasTextureIndex = m_textureLoader->load("Resources/Textures/fontConsolas.dds");

	updateTextureData();

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_frameGraphs[i] = std::make_unique<FrameGraph::Graph>(*m_renderResources->m_syncPrimitiveAllocator);
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

void VEngine::VKRenderer::render(const CommonRenderData &commonData, const RenderData &renderData, const LightData &lightData)
{
	CommonRenderData perFrameData = commonData;
	perFrameData.m_currentResourceIndex = perFrameData.m_frame % RendererConsts::FRAMES_IN_FLIGHT;
	perFrameData.m_previousResourceIndex = (perFrameData.m_frame + RendererConsts::FRAMES_IN_FLIGHT - 1) % RendererConsts::FRAMES_IN_FLIGHT;

	FrameGraph::Graph &graph = *m_frameGraphs[perFrameData.m_currentResourceIndex];
	graph.reset();

	m_descriptorSetCache->update(perFrameData.m_frame, perFrameData.m_frame - RendererConsts::FRAMES_IN_FLIGHT);

	// import resources into graph

	FrameGraph::ImageHandle shadowAtlasImageHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "Shadow Atlas";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = g_shadowAtlasSize;
		desc.m_height = g_shadowAtlasSize;
		desc.m_format = m_renderResources->m_shadowTexture.getFormat();

		shadowAtlasImageHandle = graph.importImage(desc,
			m_renderResources->m_shadowTexture.getImage(),
			m_renderResources->m_shadowTextureView,
			&m_renderResources->m_shadowTextureLayout,
			perFrameData.m_frame == 0 ? VK_NULL_HANDLE : m_renderResources->m_shadowTextureSemaphores[perFrameData.m_previousResourceIndex], // on first frame we dont wait
			m_renderResources->m_shadowTextureSemaphores[perFrameData.m_currentResourceIndex]);
	}

	FrameGraph::ImageHandle taaHistoryImageHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "TAA History Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_taaHistoryTextures[perFrameData.m_previousResourceIndex].getFormat();

		taaHistoryImageHandle = graph.importImage(desc,
			m_renderResources->m_taaHistoryTextures[perFrameData.m_previousResourceIndex].getImage(),
			m_renderResources->m_taaHistoryTextureViews[perFrameData.m_previousResourceIndex],
			&m_renderResources->m_taaHistoryTextureLayouts[perFrameData.m_previousResourceIndex],
			VK_NULL_HANDLE,
			VK_NULL_HANDLE);
	}

	FrameGraph::ImageHandle taaResolveImageHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "TAA Resolve Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_taaHistoryTextures[perFrameData.m_currentResourceIndex].getFormat();

		taaResolveImageHandle = graph.importImage(desc,
			m_renderResources->m_taaHistoryTextures[perFrameData.m_currentResourceIndex].getImage(),
			m_renderResources->m_taaHistoryTextureViews[perFrameData.m_currentResourceIndex],
			&m_renderResources->m_taaHistoryTextureLayouts[perFrameData.m_currentResourceIndex],
			VK_NULL_HANDLE,
			VK_NULL_HANDLE);
	}

	FrameGraph::BufferHandle avgLuminanceBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Average Luminance Buffer";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(float) * RendererConsts::FRAMES_IN_FLIGHT + 4;
		desc.m_hostVisible = false;

		avgLuminanceBufferHandle = graph.importBuffer(desc, m_renderResources->m_avgLuminanceBuffer.getBuffer(), m_renderResources->m_avgLuminanceBuffer.getAllocation(), VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	FrameGraph::BufferHandle materialDataBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Material Data Buffer";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(MaterialData) * RendererConsts::MAX_MATERIALS;
		desc.m_hostVisible = false;

		materialDataBufferHandle = graph.importBuffer(desc, m_renderResources->m_materialBuffer.getBuffer(), m_renderResources->m_materialBuffer.getAllocation(), VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	// create graph managed resources
	FrameGraph::ImageHandle swapchainTextureHandle = 0;
	FrameGraph::ImageHandle finalImageHandle = VKResourceDefinitions::createFinalImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle depthImageHandle = VKResourceDefinitions::createDepthImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle albedoImageHandle = VKResourceDefinitions::createAlbedoImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle normalImageHandle = VKResourceDefinitions::createNormalImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle materialImageHandle = VKResourceDefinitions::createMaterialImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle velocityImageHandle = VKResourceDefinitions::createVelocityImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle lightImageHandle = VKResourceDefinitions::createLightImageHandle(graph, m_width, m_height);
	FrameGraph::BufferHandle pointLightBitMaskBufferHandle = VKResourceDefinitions::createPointLightBitMaskBufferHandle(graph, m_width, m_height, static_cast<uint32_t>(lightData.m_pointLightData.size()));
	FrameGraph::BufferHandle transformDataBufferHandle = VKResourceDefinitions::createTransformDataBufferHandle(graph, renderData.m_transformDataCount);
	FrameGraph::BufferHandle directionalLightDataBufferHandle = VKResourceDefinitions::createDirectionalLightDataBufferHandle(graph, static_cast<uint32_t>(lightData.m_directionalLightData.size()));
	FrameGraph::BufferHandle pointLightDataBufferHandle = VKResourceDefinitions::createPointLightDataBufferHandle(graph, static_cast<uint32_t>(lightData.m_pointLightData.size()));
	FrameGraph::BufferHandle shadowDataBufferHandle = VKResourceDefinitions::createShadowDataBufferHandle(graph, static_cast<uint32_t>(lightData.m_shadowData.size()));
	FrameGraph::BufferHandle pointLightZBinsBufferHandle = VKResourceDefinitions::createPointLightZBinsBufferHandle(graph);
	FrameGraph::BufferHandle luminanceHistogramBufferHandle = VKResourceDefinitions::createLuminanceHistogramBufferHandle(graph);


	// host write passes

	VKHostWritePass::Data transformDataWriteData;
	transformDataWriteData.m_data = (unsigned char *)renderData.m_transformData;
	transformDataWriteData.m_srcOffset = 0;
	transformDataWriteData.m_dstOffset = 0;
	transformDataWriteData.m_srcItemSize = sizeof(glm::mat4) * renderData.m_transformDataCount;
	transformDataWriteData.m_dstItemSize = sizeof(glm::mat4) * renderData.m_transformDataCount;
	transformDataWriteData.m_srcItemStride = sizeof(glm::mat4) * renderData.m_transformDataCount;
	transformDataWriteData.m_count = 1;
	transformDataWriteData.m_bufferHandle = transformDataBufferHandle;

	if (renderData.m_opaqueSubMeshInstanceDataCount || renderData.m_maskedSubMeshInstanceData)
	{
		VKHostWritePass::addToGraph(graph, transformDataWriteData, "Transform Data Write Pass");
	}


	VKHostWritePass::Data directionalLightDataWriteData;
	directionalLightDataWriteData.m_data = (unsigned char *)lightData.m_directionalLightData.data();
	directionalLightDataWriteData.m_srcOffset = 0;
	directionalLightDataWriteData.m_dstOffset = 0;
	directionalLightDataWriteData.m_srcItemSize = lightData.m_directionalLightData.size() * sizeof(DirectionalLightData);
	directionalLightDataWriteData.m_dstItemSize = lightData.m_directionalLightData.size() * sizeof(DirectionalLightData);
	directionalLightDataWriteData.m_srcItemStride = lightData.m_directionalLightData.size() * sizeof(DirectionalLightData);
	directionalLightDataWriteData.m_count = 1;
	directionalLightDataWriteData.m_bufferHandle = directionalLightDataBufferHandle;

	if (!lightData.m_directionalLightData.empty())
	{
		VKHostWritePass::addToGraph(graph, directionalLightDataWriteData, "Directional Light Data Write Pass");
	}


	VKHostWritePass::Data pointLightDataWriteData;
	pointLightDataWriteData.m_data = (unsigned char *)lightData.m_pointLightData.data();
	pointLightDataWriteData.m_srcOffset = 0;
	pointLightDataWriteData.m_dstOffset = 0;
	pointLightDataWriteData.m_srcItemSize = lightData.m_pointLightData.size() * sizeof(PointLightData);
	pointLightDataWriteData.m_dstItemSize = lightData.m_pointLightData.size() * sizeof(PointLightData);
	pointLightDataWriteData.m_srcItemStride = lightData.m_pointLightData.size() * sizeof(PointLightData);
	pointLightDataWriteData.m_count = 1;
	pointLightDataWriteData.m_bufferHandle = pointLightDataBufferHandle;

	VKHostWritePass::Data pointLightZBinsWriteData;
	pointLightZBinsWriteData.m_data = (unsigned char *)lightData.m_zBins.data();
	pointLightZBinsWriteData.m_srcOffset = 0;
	pointLightZBinsWriteData.m_dstOffset = 0;
	pointLightZBinsWriteData.m_srcItemSize = lightData.m_zBins.size() * sizeof(uint32_t);
	pointLightZBinsWriteData.m_dstItemSize = lightData.m_zBins.size() * sizeof(uint32_t);
	pointLightZBinsWriteData.m_srcItemStride = lightData.m_zBins.size() * sizeof(uint32_t);
	pointLightZBinsWriteData.m_count = 1;
	pointLightZBinsWriteData.m_bufferHandle = pointLightZBinsBufferHandle;

	if (!lightData.m_pointLightData.empty())
	{
		VKHostWritePass::addToGraph(graph, pointLightDataWriteData, "Point Light Data Write Pass");
		VKHostWritePass::addToGraph(graph, pointLightZBinsWriteData, "Point Light Z-Bins Write Pass");
	}


	VKHostWritePass::Data shadowDataWriteData;
	shadowDataWriteData.m_data = (unsigned char *)lightData.m_shadowData.data();
	shadowDataWriteData.m_srcOffset = 0;
	shadowDataWriteData.m_dstOffset = 0;
	shadowDataWriteData.m_srcItemSize = lightData.m_shadowData.size() * sizeof(ShadowData);
	shadowDataWriteData.m_dstItemSize = lightData.m_shadowData.size() * sizeof(ShadowData);
	shadowDataWriteData.m_srcItemStride = lightData.m_shadowData.size() * sizeof(ShadowData);
	shadowDataWriteData.m_count = 1;
	shadowDataWriteData.m_bufferHandle = shadowDataBufferHandle;

	if (!lightData.m_shadowData.empty())
	{
		VKHostWritePass::addToGraph(graph, shadowDataWriteData, "Shadow Data Write Pass");
	}


	// draw opaque geometry to gbuffer
	VKGeometryPass::Data opaqueGeometryPassData;
	opaqueGeometryPassData.m_renderResources = m_renderResources.get();
	opaqueGeometryPassData.m_pipelineCache = m_pipelineCache.get();
	opaqueGeometryPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	opaqueGeometryPassData.m_commonRenderData = &commonData;
	opaqueGeometryPassData.m_width = m_width;
	opaqueGeometryPassData.m_height = m_height;
	opaqueGeometryPassData.m_subMeshInstanceCount = renderData.m_opaqueSubMeshInstanceDataCount;
	opaqueGeometryPassData.m_subMeshInstances = renderData.m_opaqueSubMeshInstanceData;
	opaqueGeometryPassData.m_subMeshData = renderData.m_subMeshData;
	opaqueGeometryPassData.m_alphaMasked = false;
	opaqueGeometryPassData.m_constantDataBufferHandle = 0;
	opaqueGeometryPassData.m_materialDataBufferHandle = materialDataBufferHandle;
	opaqueGeometryPassData.m_transformDataBufferHandle = transformDataBufferHandle;
	opaqueGeometryPassData.m_depthImageHandle = depthImageHandle;
	opaqueGeometryPassData.m_albedoImageHandle = albedoImageHandle;
	opaqueGeometryPassData.m_normalImageHandle = normalImageHandle;
	opaqueGeometryPassData.m_metalnessRougnessOcclusionImageHandle = materialImageHandle;
	opaqueGeometryPassData.m_velocityImageHandle = velocityImageHandle;

	if (renderData.m_opaqueSubMeshInstanceDataCount)
	{
		VKGeometryPass::addToGraph(graph, opaqueGeometryPassData);
	}


	// draw opaque alpha masked geometry to gbuffer
	VKGeometryPass::Data maskedGeometryPassData = opaqueGeometryPassData;
	maskedGeometryPassData.m_subMeshInstanceCount = renderData.m_maskedSubMeshInstanceDataCount;
	maskedGeometryPassData.m_subMeshInstances = renderData.m_maskedSubMeshInstanceData;
	maskedGeometryPassData.m_alphaMasked = true;

	if (renderData.m_maskedSubMeshInstanceDataCount)
	{
		VKGeometryPass::addToGraph(graph, maskedGeometryPassData);
	}


	// initialize velocity of static objects
	VKVelocityInitializationPass::Data velocityInitializationPassData;
	velocityInitializationPassData.m_renderResources = m_renderResources.get();
	velocityInitializationPassData.m_pipelineCache = m_pipelineCache.get();
	velocityInitializationPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	velocityInitializationPassData.m_width = m_width;
	velocityInitializationPassData.m_height = m_height;
	velocityInitializationPassData.m_reprojectionMatrix = perFrameData.m_prevViewProjectionMatrix * perFrameData.m_invViewProjectionMatrix;
	velocityInitializationPassData.m_depthImageHandle = depthImageHandle;
	velocityInitializationPassData.m_velocityImageHandle = velocityImageHandle;

	VKVelocityInitializationPass::addToGraph(graph, velocityInitializationPassData);


	// draw shadows
	VKShadowPass::Data opaqueShadowPassData;
	opaqueShadowPassData.m_renderResources = m_renderResources.get();
	opaqueShadowPassData.m_pipelineCache = m_pipelineCache.get();
	opaqueShadowPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	opaqueShadowPassData.m_width = g_shadowAtlasSize;
	opaqueShadowPassData.m_height = g_shadowAtlasSize;
	opaqueShadowPassData.m_subMeshInstanceCount = renderData.m_opaqueSubMeshInstanceDataCount;
	opaqueShadowPassData.m_subMeshInstances = renderData.m_opaqueSubMeshInstanceData;
	opaqueShadowPassData.m_subMeshData = renderData.m_subMeshData;
	opaqueShadowPassData.m_shadowJobCount = static_cast<uint32_t>(lightData.m_shadowJobs.size());
	opaqueShadowPassData.m_shadowJobs = lightData.m_shadowJobs.data();
	opaqueShadowPassData.m_alphaMasked = false;
	opaqueShadowPassData.m_clear = true;
	opaqueShadowPassData.m_transformDataBufferHandle = transformDataBufferHandle;
	opaqueShadowPassData.m_materialDataBufferHandle = materialDataBufferHandle;
	opaqueShadowPassData.m_shadowAtlasImageHandle = shadowAtlasImageHandle;

	if (renderData.m_opaqueSubMeshInstanceDataCount && !lightData.m_shadowJobs.empty())
	{
		VKShadowPass::addToGraph(graph, opaqueShadowPassData);
	}


	// draw masked shadows
	VKShadowPass::Data maskedShadowPassData;
	maskedShadowPassData.m_renderResources = m_renderResources.get();
	maskedShadowPassData.m_pipelineCache = m_pipelineCache.get();
	maskedShadowPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	maskedShadowPassData.m_width = g_shadowAtlasSize;
	maskedShadowPassData.m_height = g_shadowAtlasSize;
	maskedShadowPassData.m_subMeshInstanceCount = renderData.m_maskedSubMeshInstanceDataCount;
	maskedShadowPassData.m_subMeshInstances = renderData.m_maskedSubMeshInstanceData;
	maskedShadowPassData.m_subMeshData = renderData.m_subMeshData;
	maskedShadowPassData.m_shadowJobCount = static_cast<uint32_t>(lightData.m_shadowJobs.size());
	maskedShadowPassData.m_shadowJobs = lightData.m_shadowJobs.data();
	maskedShadowPassData.m_alphaMasked = true;
	maskedShadowPassData.m_clear = lightData.m_shadowJobs.empty();
	maskedShadowPassData.m_transformDataBufferHandle = transformDataBufferHandle;
	maskedShadowPassData.m_materialDataBufferHandle = materialDataBufferHandle;
	maskedShadowPassData.m_shadowAtlasImageHandle = shadowAtlasImageHandle;

	if (renderData.m_maskedSubMeshInstanceDataCount && !lightData.m_shadowJobs.empty())
	{
		VKShadowPass::addToGraph(graph, maskedShadowPassData);
	}


	// cull lights to tiles
	VKRasterTilingPass::Data rasterTilingPassData;
	rasterTilingPassData.m_renderResources = m_renderResources.get();
	rasterTilingPassData.m_pipelineCache = m_pipelineCache.get();
	rasterTilingPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	rasterTilingPassData.m_width = m_width;
	rasterTilingPassData.m_height = m_height;
	rasterTilingPassData.m_lightData = &lightData;
	rasterTilingPassData.m_projection = perFrameData.m_jitteredProjectionMatrix;
	rasterTilingPassData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferHandle;

	if (!lightData.m_pointLightData.empty())
	{
		VKRasterTilingPass::addToGraph(graph, rasterTilingPassData);
	}


	// light gbuffer
	VKLightingPass::Data lightingPassData;
	lightingPassData.m_renderResources = m_renderResources.get();
	lightingPassData.m_pipelineCache = m_pipelineCache.get();
	lightingPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	lightingPassData.m_commonRenderData = &commonData;
	lightingPassData.m_width = m_width;
	lightingPassData.m_height = m_height;
	lightingPassData.m_constantDataBufferHandle = 0;
	lightingPassData.m_directionalLightDataBufferHandle = directionalLightDataBufferHandle;
	lightingPassData.m_pointLightDataBufferHandle = pointLightDataBufferHandle;
	lightingPassData.m_shadowDataBufferHandle = shadowDataBufferHandle;
	lightingPassData.m_pointLightZBinsBufferHandle = pointLightZBinsBufferHandle;
	lightingPassData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferHandle;
	lightingPassData.m_depthImageHandle = depthImageHandle;
	lightingPassData.m_albedoImageHandle = albedoImageHandle;
	lightingPassData.m_normalImageHandle = normalImageHandle;
	lightingPassData.m_metalnessRougnessOcclusionImageHandle = materialImageHandle;
	lightingPassData.m_shadowAtlasImageHandle = shadowAtlasImageHandle;
	lightingPassData.m_resultImageHandle = lightImageHandle;

	VKLightingPass::addToGraph(graph, lightingPassData);


	// calculate luminance histograms
	VKLuminanceHistogramPass::Data luminanceHistogramPassData;
	luminanceHistogramPassData.m_renderResources = m_renderResources.get();
	luminanceHistogramPassData.m_pipelineCache = m_pipelineCache.get();
	luminanceHistogramPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	luminanceHistogramPassData.m_width = m_width;
	luminanceHistogramPassData.m_height = m_height;
	luminanceHistogramPassData.m_logMin = -10.0f;
	luminanceHistogramPassData.m_logMax = 17.0f;
	luminanceHistogramPassData.m_lightImageHandle = lightImageHandle;
	luminanceHistogramPassData.m_luminanceHistogramBufferHandle = luminanceHistogramBufferHandle;

	VKLuminanceHistogramPass::addToGraph(graph, luminanceHistogramPassData);


	// calculate avg luminance
	VKLuminanceHistogramAveragePass::Data luminanceHistogramAvgPassData;
	luminanceHistogramAvgPassData.m_renderResources = m_renderResources.get();
	luminanceHistogramAvgPassData.m_pipelineCache = m_pipelineCache.get();
	luminanceHistogramAvgPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	luminanceHistogramAvgPassData.m_width = m_width;
	luminanceHistogramAvgPassData.m_height = m_height;
	luminanceHistogramAvgPassData.m_timeDelta = commonData.m_timeDelta;
	luminanceHistogramAvgPassData.m_logMin = -10.0f;
	luminanceHistogramAvgPassData.m_logMax = 17.0f;
	luminanceHistogramAvgPassData.m_currentResourceIndex = commonData.m_currentResourceIndex;
	luminanceHistogramAvgPassData.m_previousResourceIndex = commonData.m_previousResourceIndex;
	luminanceHistogramAvgPassData.m_luminanceHistogramBufferHandle = luminanceHistogramBufferHandle;
	luminanceHistogramAvgPassData.m_avgLuminanceBufferHandle = avgLuminanceBufferHandle;

	VKLuminanceHistogramAveragePass::addToGraph(graph, luminanceHistogramAvgPassData);


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
	VKTAAResolvePass::Data taaResolvePassData;
	taaResolvePassData.m_renderResources = m_renderResources.get();
	taaResolvePassData.m_pipelineCache = m_pipelineCache.get();
	taaResolvePassData.m_descriptorSetCache = m_descriptorSetCache.get();
	taaResolvePassData.m_width = m_width;
	taaResolvePassData.m_height = m_height;
	taaResolvePassData.m_jitterOffsetX = commonData.m_jitteredProjectionMatrix[2][0];
	taaResolvePassData.m_jitterOffsetY = commonData.m_jitteredProjectionMatrix[2][1];
	taaResolvePassData.m_depthImageHandle = depthImageHandle;
	taaResolvePassData.m_velocityImageHandle = velocityImageHandle;
	taaResolvePassData.m_taaHistoryImageHandle = taaHistoryImageHandle;
	taaResolvePassData.m_lightImageHandle = lightImageHandle;
	taaResolvePassData.m_taaResolveImageHandle = taaResolveImageHandle;

	if (g_TAAEnabled)
	{
		VKTAAResolvePass::addToGraph(graph, taaResolvePassData);

		lightImageHandle = taaResolveImageHandle;
	}


	FrameGraph::ImageHandle tonemapTargetTextureHandle = m_swapChain->getImageFormat() != VK_FORMAT_R8G8B8A8_UNORM ? finalImageHandle : swapchainTextureHandle;

	// tonemap
	VKTonemapPass::Data tonemapPassData;
	tonemapPassData.m_renderResources = m_renderResources.get();
	tonemapPassData.m_pipelineCache = m_pipelineCache.get();
	tonemapPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	tonemapPassData.m_width = m_width;
	tonemapPassData.m_height = m_height;
	tonemapPassData.m_resourceIndex = commonData.m_currentResourceIndex;
	tonemapPassData.m_srcImageHandle = lightImageHandle;
	tonemapPassData.m_dstImageHandle = tonemapTargetTextureHandle;
	tonemapPassData.m_avgLuminanceBufferHandle = avgLuminanceBufferHandle;

	VKTonemapPass::addToGraph(graph, tonemapPassData);


	// luminance debug pass
	VKLuminanceHistogramDebugPass::Data luminanceDebugPassData;
	luminanceDebugPassData.m_renderResources = m_renderResources.get();
	luminanceDebugPassData.m_pipelineCache = m_pipelineCache.get();
	luminanceDebugPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	luminanceDebugPassData.m_width = m_width;
	luminanceDebugPassData.m_height = m_height;
	luminanceDebugPassData.m_offsetX = 0.5f;
	luminanceDebugPassData.m_offsetY = 0.0f;
	luminanceDebugPassData.m_scaleX = 0.5f;
	luminanceDebugPassData.m_scaleY = 1.0f;
	luminanceDebugPassData.m_colorImageHandle = tonemapTargetTextureHandle;
	luminanceDebugPassData.m_luminanceHistogramBufferHandle = luminanceHistogramBufferHandle;

	//VKLuminanceHistogramDebugPass::addToGraph(graph, luminanceDebugPassData);
	

	// memory heap debug pass
	VKMemoryHeapDebugPass::Data memoryHeapDebugPassData;
	memoryHeapDebugPassData.m_renderResources = m_renderResources.get();
	memoryHeapDebugPassData.m_pipelineCache = m_pipelineCache.get();
	memoryHeapDebugPassData.m_width = m_width;
	memoryHeapDebugPassData.m_height = m_height;
	memoryHeapDebugPassData.m_offsetX = 0.75f;
	memoryHeapDebugPassData.m_offsetY = 0.0f;
	memoryHeapDebugPassData.m_scaleX = 0.25f;
	memoryHeapDebugPassData.m_scaleY = 0.25f;
	memoryHeapDebugPassData.m_colorImageHandle = tonemapTargetTextureHandle;

	//VKMemoryHeapDebugPass::addToGraph(graph, memoryHeapDebugPassData);
	

	// text pass
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

	VKTextPass::Data textPassData;
	textPassData.m_renderResources = m_renderResources.get();
	textPassData.m_pipelineCache = m_pipelineCache.get();
	textPassData.m_width = m_width;
	textPassData.m_height = m_height;
	textPassData.m_atlasTextureIndex = m_fontAtlasTextureIndex;
	textPassData.m_stringCount = timingInfoCount;
	textPassData.m_strings = timingInfoStrings;
	textPassData.m_colorImageHandle = tonemapTargetTextureHandle;

	VKTextPass::addToGraph(graph, textPassData);


	// blit to swapchain image
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

	VKBlitPass::Data finalBlitPassData;
	finalBlitPassData.m_regionCount = 1;
	finalBlitPassData.m_regions = &imageBlitRegion;
	finalBlitPassData.m_filter = VK_FILTER_NEAREST;
	finalBlitPassData.m_srcImage = tonemapTargetTextureHandle;
	finalBlitPassData.m_dstImage = swapchainTextureHandle;

	if (m_swapChain->getImageFormat() != VK_FORMAT_R8G8B8A8_UNORM)
	{
		VKBlitPass::addToGraph(graph, finalBlitPassData, "Final Blit Pass");
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

VEngine::TextureHandle VEngine::VKRenderer::loadTexture(const char *filepath)
{
	return m_textureLoader->load(filepath);
}

void VEngine::VKRenderer::freeTexture(TextureHandle id)
{
	m_textureLoader->free(id);
}

void VEngine::VKRenderer::createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	m_materialManager->createMaterials(count, materials, handles);
}

void VEngine::VKRenderer::updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	m_materialManager->updateMaterials(count, materials, handles);
}

void VEngine::VKRenderer::destroyMaterials(uint32_t count, MaterialHandle *handles)
{
	m_materialManager->destroyMaterials(count, handles);
}

void VEngine::VKRenderer::updateTextureData()
{
	const VkDescriptorImageInfo *imageInfos;
	size_t count;
	m_textureLoader->getDescriptorImageInfos(&imageInfos, count);
	m_renderResources->updateTextureArray(imageInfos, count);
}