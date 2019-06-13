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
#include "Pass/VKPrepareIndirectBuffersPass.h"
#include "Pass/VKGeometryPass.h"
#include "Pass/VKShadowPass.h"
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
#include "Pass/VKFXAAPass.h"
#include "Pass/VKTransparencyWritePass.h"
#include "Pass/VKGTAOPass.h"
#include "Pass/VKGTAOSpatialFilterPass.h"
#include "Pass/VKGTAOTemporalFilterPass.h"
#include "Pass/VKSDSMShadowMatrixPass.h"
#include "Pass/VKDirectLightingPass.h"
#include "Module/VKSDSMModule.h"
#include "VKPipelineCache.h"
#include "VKDescriptorSetCache.h"
#include "VKMaterialManager.h"
#include "VKMeshManager.h"
#include "VKResourceDefinitions.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

VEngine::VKRenderer::VKRenderer(uint32_t width, uint32_t height, void *windowHandle)
{
	g_context.init(static_cast<GLFWwindow *>(windowHandle));

	m_pipelineCache = std::make_unique<VKPipelineCache>();
	m_descriptorSetCache = std::make_unique<VKDescriptorSetCache>();
	m_renderResources = std::make_unique<VKRenderResources>();
	m_textureLoader = std::make_unique<VKTextureLoader>(m_renderResources->m_stagingBuffer);
	m_materialManager = std::make_unique<VKMaterialManager>(m_renderResources->m_stagingBuffer, m_renderResources->m_materialBuffer);
	m_meshManager = std::make_unique<VKMeshManager>(m_renderResources->m_stagingBuffer, m_renderResources->m_vertexBuffer, m_renderResources->m_indexBuffer, m_renderResources->m_subMeshDataInfoBuffer);
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
		m_frameGraphs[i].reset();
	}
	m_swapChain.reset();
	m_textureLoader.reset();
	m_renderResources.reset();
}

void VEngine::VKRenderer::render(const CommonRenderData &commonData, const RenderData &renderData, const LightData &lightData)
{
	FrameGraph::Graph &graph = *m_frameGraphs[commonData.m_currentResourceIndex];

	// reset per frame resources
	graph.reset();
	m_renderResources->m_mappableUBOBlock[commonData.m_currentResourceIndex]->reset();
	m_renderResources->m_mappableSSBOBlock[commonData.m_currentResourceIndex]->reset();
	m_descriptorSetCache->update(commonData.m_frame, commonData.m_frame - RendererConsts::FRAMES_IN_FLIGHT);

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
			commonData.m_frame == 0 ? VK_NULL_HANDLE : m_renderResources->m_shadowTextureSemaphores[commonData.m_previousResourceIndex], // on first frame we dont wait
			m_renderResources->m_shadowTextureSemaphores[commonData.m_currentResourceIndex]);
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
		desc.m_format = m_renderResources->m_taaHistoryTextures[commonData.m_previousResourceIndex].getFormat();

		taaHistoryImageHandle = graph.importImage(desc,
			m_renderResources->m_taaHistoryTextures[commonData.m_previousResourceIndex].getImage(),
			m_renderResources->m_taaHistoryTextureViews[commonData.m_previousResourceIndex],
			&m_renderResources->m_taaHistoryTextureLayouts[commonData.m_previousResourceIndex],
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
		desc.m_format = m_renderResources->m_taaHistoryTextures[commonData.m_currentResourceIndex].getFormat();

		taaResolveImageHandle = graph.importImage(desc,
			m_renderResources->m_taaHistoryTextures[commonData.m_currentResourceIndex].getImage(),
			m_renderResources->m_taaHistoryTextureViews[commonData.m_currentResourceIndex],
			&m_renderResources->m_taaHistoryTextureLayouts[commonData.m_currentResourceIndex],
			VK_NULL_HANDLE,
			VK_NULL_HANDLE);
	}

	FrameGraph::ImageHandle gtaoPreviousImageHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "GTAO Previous Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_gtaoHistoryTextures[commonData.m_previousResourceIndex].getFormat();

		gtaoPreviousImageHandle = graph.importImage(desc,
			m_renderResources->m_gtaoHistoryTextures[commonData.m_previousResourceIndex].getImage(),
			m_renderResources->m_gtaoHistoryTextureViews[commonData.m_previousResourceIndex],
			&m_renderResources->m_gtaoHistoryTextureLayouts[commonData.m_previousResourceIndex],
			VK_NULL_HANDLE,
			VK_NULL_HANDLE);
	}

	FrameGraph::ImageHandle gtaoImageHandle = 0;
	{
		FrameGraph::ImageDescription desc = {};
		desc.m_name = "GTAO Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_gtaoHistoryTextures[commonData.m_currentResourceIndex].getFormat();

		gtaoImageHandle = graph.importImage(desc,
			m_renderResources->m_gtaoHistoryTextures[commonData.m_currentResourceIndex].getImage(),
			m_renderResources->m_gtaoHistoryTextureViews[commonData.m_currentResourceIndex],
			&m_renderResources->m_gtaoHistoryTextureLayouts[commonData.m_currentResourceIndex],
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

		avgLuminanceBufferHandle = graph.importBuffer(desc, m_renderResources->m_avgLuminanceBuffer.getBuffer(), 0, m_renderResources->m_avgLuminanceBuffer.getAllocation(), VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	// create graph managed resources
	FrameGraph::ImageHandle swapchainTextureHandle = 0;
	FrameGraph::ImageHandle finalImageHandle = VKResourceDefinitions::createFinalImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle depthImageHandle = VKResourceDefinitions::createDepthImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle uvImageHandle = VKResourceDefinitions::createUVImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle ddxyLengthImageHandle = VKResourceDefinitions::createDerivativesLengthImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle ddxyRotMaterialIdImageHandle = VKResourceDefinitions::createDerivativesRotMaterialIdImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle tangentSpaceImageHandle = VKResourceDefinitions::createTangentSpaceImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle velocityImageHandle = VKResourceDefinitions::createVelocityImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle lightImageHandle = VKResourceDefinitions::createLightImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle transparencyAccumImageHandle = VKResourceDefinitions::createTransparencyAccumImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle transparencyTransmittanceImageHandle = VKResourceDefinitions::createTransparencyTransmittanceImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle transparencyDeltaImageHandle = VKResourceDefinitions::createTransparencyDeltaImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle transparencyResultImageHandle = VKResourceDefinitions::createLightImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle gtaoRawImageHandle = VKResourceDefinitions::createGTAOImageHandle(graph, m_width, m_height);
	FrameGraph::ImageHandle gtaoSpatiallyFilteredImageHandle = VKResourceDefinitions::createGTAOImageHandle(graph, m_width, m_height);
	FrameGraph::BufferHandle pointLightBitMaskBufferHandle = VKResourceDefinitions::createPointLightBitMaskBufferHandle(graph, m_width, m_height, static_cast<uint32_t>(lightData.m_pointLightData.size()));
	FrameGraph::BufferHandle luminanceHistogramBufferHandle = VKResourceDefinitions::createLuminanceHistogramBufferHandle(graph);
	FrameGraph::BufferHandle opaqueIndirectBufferHandle = VKResourceDefinitions::createIndirectBufferHandle(graph, renderData.m_opaqueBatchSize);
	FrameGraph::BufferHandle maskedIndirectBufferHandle = VKResourceDefinitions::createIndirectBufferHandle(graph, renderData.m_alphaTestedBatchSize);
	FrameGraph::BufferHandle transparentIndirectBufferHandle = VKResourceDefinitions::createIndirectBufferHandle(graph, renderData.m_transparentBatchSize);
	FrameGraph::BufferHandle opaqueShadowIndirectBufferHandle = VKResourceDefinitions::createIndirectBufferHandle(graph, renderData.m_opaqueShadowBatchSize);
	FrameGraph::BufferHandle maskedShadowIndirectBufferHandle = VKResourceDefinitions::createIndirectBufferHandle(graph, renderData.m_alphaTestedShadowBatchSize);

	// transform data write
	VkDescriptorBufferInfo transformDataBufferInfo{ VK_NULL_HANDLE, 0, std::max(renderData.m_transformDataCount * sizeof(glm::mat4), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_currentResourceIndex]->allocate(transformDataBufferInfo.range, transformDataBufferInfo.offset, transformDataBufferInfo.buffer, bufferPtr);
		if (renderData.m_transformDataCount)
		{
			memcpy(bufferPtr, renderData.m_transformData, renderData.m_transformDataCount * sizeof(glm::mat4));
		}
	}

	// directional light data write
	VkDescriptorBufferInfo directionalLightDataBufferInfo{ VK_NULL_HANDLE, 0, std::max(lightData.m_directionalLightData.size() * sizeof(DirectionalLightData), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_currentResourceIndex]->allocate(directionalLightDataBufferInfo.range, directionalLightDataBufferInfo.offset, directionalLightDataBufferInfo.buffer, bufferPtr);
		if (!lightData.m_directionalLightData.empty())
		{
			memcpy(bufferPtr, lightData.m_directionalLightData.data(), lightData.m_directionalLightData.size() * sizeof(DirectionalLightData));
		}
	}

	// point light data write
	VkDescriptorBufferInfo pointLightDataBufferInfo{ VK_NULL_HANDLE, 0, std::max(lightData.m_pointLightData.size() * sizeof(PointLightData), size_t(1)) };
	VkDescriptorBufferInfo pointLightZBinsBufferInfo{ VK_NULL_HANDLE, 0, std::max(lightData.m_zBins.size() * sizeof(uint32_t), size_t(1)) };
	{
		uint8_t *dataBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_currentResourceIndex]->allocate(pointLightDataBufferInfo.range, pointLightDataBufferInfo.offset, pointLightDataBufferInfo.buffer, dataBufferPtr);
		uint8_t *zBinsBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_currentResourceIndex]->allocate(pointLightZBinsBufferInfo.range, pointLightZBinsBufferInfo.offset, pointLightZBinsBufferInfo.buffer, zBinsBufferPtr);
		if (!lightData.m_pointLightData.empty())
		{
			memcpy(dataBufferPtr, lightData.m_pointLightData.data(), lightData.m_pointLightData.size() * sizeof(PointLightData));
			memcpy(zBinsBufferPtr, lightData.m_zBins.data(), lightData.m_zBins.size() * sizeof(uint32_t));
		}
	}

	// shadow data write
	VkDescriptorBufferInfo shadowDataBufferInfo{ VK_NULL_HANDLE, 0, std::max(lightData.m_shadowData.size() * sizeof(ShadowData), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_currentResourceIndex]->allocate(shadowDataBufferInfo.range, shadowDataBufferInfo.offset, shadowDataBufferInfo.buffer, bufferPtr);
		if (!lightData.m_shadowData.empty())
		{
			memcpy(bufferPtr, lightData.m_shadowData.data(), lightData.m_shadowData.size() * sizeof(ShadowData));
		}
	}

	FrameGraph::BufferHandle shadowDataBufferHandle = 0;
	{
		FrameGraph::BufferDescription desc = {};
		desc.m_name = "Shadow Data Buffer";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = shadowDataBufferInfo.range;
		desc.m_hostVisible = false;

		shadowDataBufferHandle = graph.importBuffer(desc, shadowDataBufferInfo.buffer, shadowDataBufferInfo.offset, m_renderResources->m_ssboBuffers[commonData.m_currentResourceIndex].getAllocation(), VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	// instance data write
	VkDescriptorBufferInfo instanceDataBufferInfo{ VK_NULL_HANDLE, 0, std::max((renderData.m_opaqueBatchSize 
		+ renderData.m_alphaTestedBatchSize 
		+ renderData.m_transparentBatchSize
		+ renderData.m_opaqueShadowBatchSize 
		+ renderData.m_alphaTestedShadowBatchSize) * sizeof(SubMeshInstanceData), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_currentResourceIndex]->allocate(instanceDataBufferInfo.range, instanceDataBufferInfo.offset, instanceDataBufferInfo.buffer, bufferPtr);
		if (renderData.m_opaqueBatchSize)
		{
			memcpy(bufferPtr, renderData.m_opaqueBatch, renderData.m_opaqueBatchSize * sizeof(SubMeshInstanceData));
			bufferPtr += renderData.m_opaqueBatchSize * sizeof(SubMeshInstanceData);
		}
		if (renderData.m_alphaTestedBatchSize)
		{
			memcpy(bufferPtr, renderData.m_alphaTestedBatch, renderData.m_alphaTestedBatchSize * sizeof(SubMeshInstanceData));
			bufferPtr += renderData.m_alphaTestedBatchSize * sizeof(SubMeshInstanceData);
		}
		if (renderData.m_transparentBatchSize)
		{
			memcpy(bufferPtr, renderData.m_transparentBatch, renderData.m_transparentBatchSize * sizeof(SubMeshInstanceData));
			bufferPtr += renderData.m_transparentBatchSize * sizeof(SubMeshInstanceData);
		}
		if (renderData.m_opaqueShadowBatchSize)
		{
			memcpy(bufferPtr, renderData.m_opaqueShadowBatch, renderData.m_opaqueShadowBatchSize * sizeof(SubMeshInstanceData));
			bufferPtr += renderData.m_opaqueShadowBatchSize * sizeof(SubMeshInstanceData);
		}
		if (renderData.m_alphaTestedShadowBatchSize)
		{
			memcpy(bufferPtr, renderData.m_alphaTestedShadowBatch, renderData.m_alphaTestedShadowBatchSize * sizeof(SubMeshInstanceData));
			bufferPtr += renderData.m_alphaTestedShadowBatchSize * sizeof(SubMeshInstanceData);
		}
	}

	// prepare indirect buffers
	VKPrepareIndirectBuffersPass::Data prepareIndirectBuffersPassData;
	prepareIndirectBuffersPassData.m_pipelineCache = m_pipelineCache.get();
	prepareIndirectBuffersPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	prepareIndirectBuffersPassData.m_opaqueCount = renderData.m_opaqueBatchSize;
	prepareIndirectBuffersPassData.m_maskedCount = renderData.m_alphaTestedBatchSize;
	prepareIndirectBuffersPassData.m_transparentCount = renderData.m_transparentBatchSize;
	prepareIndirectBuffersPassData.m_opaqueShadowCount = renderData.m_opaqueShadowBatchSize;
	prepareIndirectBuffersPassData.m_maskedShadowCount = renderData.m_alphaTestedShadowBatchSize;
	prepareIndirectBuffersPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	prepareIndirectBuffersPassData.m_subMeshDataBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
	prepareIndirectBuffersPassData.m_opaqueIndirectBufferHandle = opaqueIndirectBufferHandle;
	prepareIndirectBuffersPassData.m_maskedIndirectBufferHandle = maskedIndirectBufferHandle;
	prepareIndirectBuffersPassData.m_transparentIndirectBufferHandle = transparentIndirectBufferHandle;
	prepareIndirectBuffersPassData.m_opaqueShadowIndirectBufferHandle = opaqueShadowIndirectBufferHandle;
	prepareIndirectBuffersPassData.m_maskedShadowIndirectBufferHandle = maskedShadowIndirectBufferHandle;

	if (renderData.m_opaqueBatchSize || renderData.m_alphaTestedBatchSize || renderData.m_opaqueShadowBatchSize || renderData.m_alphaTestedShadowBatchSize)
	{
		VKPrepareIndirectBuffersPass::addToGraph(graph, prepareIndirectBuffersPassData);
	}

	// draw opaque geometry to gbuffer
	VKGeometryPass::Data opaqueGeometryPassData;
	opaqueGeometryPassData.m_renderResources = m_renderResources.get();
	opaqueGeometryPassData.m_pipelineCache = m_pipelineCache.get();
	opaqueGeometryPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	opaqueGeometryPassData.m_width = m_width;
	opaqueGeometryPassData.m_height = m_height;
	opaqueGeometryPassData.m_drawCount = renderData.m_opaqueBatchSize;
	opaqueGeometryPassData.m_alphaMasked = false;
	opaqueGeometryPassData.m_viewMatrix = commonData.m_viewMatrix;
	opaqueGeometryPassData.m_jitteredViewProjectionMatrix = commonData.m_jitteredViewProjectionMatrix;
	opaqueGeometryPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	opaqueGeometryPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
	opaqueGeometryPassData.m_transformDataBufferInfo = transformDataBufferInfo;
	opaqueGeometryPassData.m_indirectBufferHandle = opaqueIndirectBufferHandle;
	opaqueGeometryPassData.m_depthImageHandle = depthImageHandle;
	opaqueGeometryPassData.m_uvImageHandle = uvImageHandle;
	opaqueGeometryPassData.m_ddxyLengthImageHandle = ddxyLengthImageHandle;
	opaqueGeometryPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageHandle;
	opaqueGeometryPassData.m_tangentSpaceImageHandle = tangentSpaceImageHandle;

	if (renderData.m_opaqueBatchSize)
	{
		VKGeometryPass::addToGraph(graph, opaqueGeometryPassData);
	}


	// draw alpha masked geometry to gbuffer
	VKGeometryPass::Data maskedGeometryPassData = opaqueGeometryPassData;
	maskedGeometryPassData.m_drawCount = renderData.m_alphaTestedBatchSize;
	maskedGeometryPassData.m_alphaMasked = true;
	maskedGeometryPassData.m_indirectBufferHandle = maskedIndirectBufferHandle;
	
	if (renderData.m_alphaTestedBatchSize)
	{
		VKGeometryPass::addToGraph(graph, maskedGeometryPassData);
	}

	// common sdsm
	VKSDSMModule::OutputData sdsmOutputData;
	VKSDSMModule::InputData sdsmInputData;
	sdsmInputData.m_renderResources = m_renderResources.get();
	sdsmInputData.m_pipelineCache = m_pipelineCache.get();
	sdsmInputData.m_descriptorSetCache = m_descriptorSetCache.get();
	sdsmInputData.m_width = m_width;
	sdsmInputData.m_height = m_height;
	sdsmInputData.m_nearPlane = commonData.m_nearPlane;
	sdsmInputData.m_farPlane = commonData.m_farPlane;
	sdsmInputData.m_invProjection = commonData.m_invJitteredProjectionMatrix;
	sdsmInputData.m_depthImageHandle = depthImageHandle;

	VKSDSMModule::addToGraph(graph, sdsmInputData, sdsmOutputData);


	// sdsm shadow matrix
	VKSDSMShadowMatrixPass::Data sdsmShadowMatrixPassData;
	sdsmShadowMatrixPassData.m_renderResources = m_renderResources.get();
	sdsmShadowMatrixPassData.m_pipelineCache = m_pipelineCache.get();
	sdsmShadowMatrixPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	sdsmShadowMatrixPassData.m_lightView = glm::lookAt(glm::vec3(), -glm::vec3(commonData.m_invViewMatrix * lightData.m_directionalLightData.front().m_direction), glm::vec3(glm::transpose(commonData.m_viewMatrix)[0]));
	sdsmShadowMatrixPassData.m_cameraViewToLightView = sdsmShadowMatrixPassData.m_lightView * commonData.m_invViewProjectionMatrix;
	sdsmShadowMatrixPassData.m_lightSpaceNear = renderData.m_orthoNearest;
	sdsmShadowMatrixPassData.m_lightSpaceFar = renderData.m_orthoFarthest;
	sdsmShadowMatrixPassData.m_shadowDataBufferHandle = shadowDataBufferHandle;
	sdsmShadowMatrixPassData.m_partitionBoundsBufferHandle = sdsmOutputData.m_partitionBoundsBufferHandle;

	VKSDSMShadowMatrixPass::addToGraph(graph, sdsmShadowMatrixPassData);


	// initialize velocity of static objects
	VKVelocityInitializationPass::Data velocityInitializationPassData;
	velocityInitializationPassData.m_renderResources = m_renderResources.get();
	velocityInitializationPassData.m_pipelineCache = m_pipelineCache.get();
	velocityInitializationPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	velocityInitializationPassData.m_width = m_width;
	velocityInitializationPassData.m_height = m_height;
	velocityInitializationPassData.m_reprojectionMatrix = commonData.m_prevViewProjectionMatrix * commonData.m_invViewProjectionMatrix;
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
	opaqueShadowPassData.m_drawCount = renderData.m_opaqueShadowBatchSize;
	opaqueShadowPassData.m_shadowJobCount = static_cast<uint32_t>(lightData.m_shadowJobs.size());
	opaqueShadowPassData.m_shadowJobs = lightData.m_shadowJobs.data();
	opaqueShadowPassData.m_alphaMasked = false;
	opaqueShadowPassData.m_clear = true;
	opaqueShadowPassData.m_instanceDataBufferInfo = opaqueGeometryPassData.m_instanceDataBufferInfo;
	opaqueShadowPassData.m_materialDataBufferInfo = opaqueGeometryPassData.m_materialDataBufferInfo;
	opaqueShadowPassData.m_transformDataBufferInfo = opaqueGeometryPassData.m_transformDataBufferInfo;
	opaqueShadowPassData.m_shadowDataBufferHandle = shadowDataBufferHandle;
	opaqueShadowPassData.m_indirectBufferHandle = opaqueShadowIndirectBufferHandle;
	opaqueShadowPassData.m_shadowAtlasImageHandle = shadowAtlasImageHandle;

	if (renderData.m_opaqueShadowBatchSize && !lightData.m_shadowJobs.empty())
	{
		VKShadowPass::addToGraph(graph, opaqueShadowPassData);
	}


	// draw masked shadows
	VKShadowPass::Data maskedShadowPassData = opaqueShadowPassData;
	maskedShadowPassData.m_drawCount = renderData.m_alphaTestedShadowBatchSize;
	maskedShadowPassData.m_alphaMasked = true;
	maskedShadowPassData.m_clear = lightData.m_shadowJobs.empty();
	maskedShadowPassData.m_indirectBufferHandle = maskedShadowIndirectBufferHandle;

	if (renderData.m_alphaTestedShadowBatchSize && !lightData.m_shadowJobs.empty())
	{
		VKShadowPass::addToGraph(graph, maskedShadowPassData);
	}


	// gtao
	VKGTAOPass::Data gtaoPassData;
	gtaoPassData.m_renderResources = m_renderResources.get();
	gtaoPassData.m_pipelineCache = m_pipelineCache.get();
	gtaoPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	gtaoPassData.m_width = m_width;
	gtaoPassData.m_height = m_height;
	gtaoPassData.m_frame = commonData.m_frame;
	gtaoPassData.m_fovy = commonData.m_fovy;
	gtaoPassData.m_invProjection = commonData.m_invJitteredProjectionMatrix;
	gtaoPassData.m_depthImageHandle = depthImageHandle;
	gtaoPassData.m_resultImageHandle = gtaoRawImageHandle;

	if (g_ssaoEnabled)
	{
		VKGTAOPass::addToGraph(graph, gtaoPassData);
	}


	// gtao spatial filter
	VKGTAOSpatialFilterPass::Data gtaoPassSpatialFilterPassData;
	gtaoPassSpatialFilterPassData.m_renderResources = m_renderResources.get();
	gtaoPassSpatialFilterPassData.m_pipelineCache = m_pipelineCache.get();
	gtaoPassSpatialFilterPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	gtaoPassSpatialFilterPassData.m_width = m_width;
	gtaoPassSpatialFilterPassData.m_height = m_height;
	gtaoPassSpatialFilterPassData.m_inputImageHandle = gtaoRawImageHandle;
	gtaoPassSpatialFilterPassData.m_resultImageHandle = gtaoSpatiallyFilteredImageHandle;

	if (g_ssaoEnabled)
	{
		VKGTAOSpatialFilterPass::addToGraph(graph, gtaoPassSpatialFilterPassData);
	}


	// gtao temporal filter
	VKGTAOTemporalFilterPass::Data gtaoPassTemporalFilterPassData;
	gtaoPassTemporalFilterPassData.m_renderResources = m_renderResources.get();
	gtaoPassTemporalFilterPassData.m_pipelineCache = m_pipelineCache.get();
	gtaoPassTemporalFilterPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	gtaoPassTemporalFilterPassData.m_width = m_width;
	gtaoPassTemporalFilterPassData.m_height = m_height;
	gtaoPassTemporalFilterPassData.m_nearPlane = commonData.m_nearPlane;
	gtaoPassTemporalFilterPassData.m_farPlane = commonData.m_farPlane;
	gtaoPassTemporalFilterPassData.m_invViewProjection = commonData.m_invJitteredViewProjectionMatrix;
	gtaoPassTemporalFilterPassData.m_prevInvViewProjection = commonData.m_prevInvJitteredViewProjectionMatrix;
	gtaoPassTemporalFilterPassData.m_inputImageHandle = gtaoSpatiallyFilteredImageHandle;
	gtaoPassTemporalFilterPassData.m_velocityImageHandle = velocityImageHandle;
	gtaoPassTemporalFilterPassData.m_previousImageHandle = gtaoPreviousImageHandle;
	gtaoPassTemporalFilterPassData.m_resultImageHandle = gtaoImageHandle;

	if (g_ssaoEnabled)
	{
		VKGTAOTemporalFilterPass::addToGraph(graph, gtaoPassTemporalFilterPassData);
	}


	// cull lights to tiles
	VKRasterTilingPass::Data rasterTilingPassData;
	rasterTilingPassData.m_renderResources = m_renderResources.get();
	rasterTilingPassData.m_pipelineCache = m_pipelineCache.get();
	rasterTilingPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	rasterTilingPassData.m_width = m_width;
	rasterTilingPassData.m_height = m_height;
	rasterTilingPassData.m_lightData = &lightData;
	rasterTilingPassData.m_projection = commonData.m_jitteredProjectionMatrix;
	rasterTilingPassData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferHandle;

	if (!lightData.m_pointLightData.empty())
	{
		VKRasterTilingPass::addToGraph(graph, rasterTilingPassData);
	}


	// light gbuffer
	VKDirectLightingPass::Data lightingPassData;
	lightingPassData.m_renderResources = m_renderResources.get();
	lightingPassData.m_pipelineCache = m_pipelineCache.get();
	lightingPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	lightingPassData.m_commonRenderData = &commonData;
	lightingPassData.m_width = m_width;
	lightingPassData.m_height = m_height;
	lightingPassData.m_ssao = g_ssaoEnabled;
	lightingPassData.m_directionalLightDataBufferInfo = directionalLightDataBufferInfo;
	lightingPassData.m_pointLightDataBufferInfo = pointLightDataBufferInfo;
	lightingPassData.m_pointLightZBinsBufferInfo = pointLightZBinsBufferInfo;
	lightingPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
	lightingPassData.m_shadowDataBufferHandle = shadowDataBufferHandle;
	lightingPassData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferHandle;
	lightingPassData.m_depthImageHandle = depthImageHandle;
	lightingPassData.m_uvImageHandle = uvImageHandle;
	lightingPassData.m_ddxyLengthImageHandle = ddxyLengthImageHandle;
	lightingPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageHandle;
	lightingPassData.m_tangentSpaceImageHandle = tangentSpaceImageHandle;
	lightingPassData.m_shadowAtlasImageHandle = shadowAtlasImageHandle;
	lightingPassData.m_occlusionImageHandle = gtaoImageHandle;
	lightingPassData.m_resultImageHandle = lightImageHandle;

	VKDirectLightingPass::addToGraph(graph, lightingPassData);


	VKTransparencyWritePass::Data transparencyWriteData;
	transparencyWriteData.m_renderResources = m_renderResources.get();
	transparencyWriteData.m_pipelineCache = m_pipelineCache.get();
	transparencyWriteData.m_descriptorSetCache = m_descriptorSetCache.get();
	transparencyWriteData.m_commonRenderData = &commonData;
	transparencyWriteData.m_width = m_width;
	transparencyWriteData.m_height = m_height;
	transparencyWriteData.m_drawCount = renderData.m_transparentBatchSize;
	transparencyWriteData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	transparencyWriteData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
	transparencyWriteData.m_transformDataBufferInfo = transformDataBufferInfo;
	transparencyWriteData.m_directionalLightDataBufferInfo = directionalLightDataBufferInfo;
	transparencyWriteData.m_pointLightDataBufferInfo = pointLightDataBufferInfo;
	transparencyWriteData.m_shadowDataBufferInfo = shadowDataBufferInfo;
	transparencyWriteData.m_pointLightZBinsBufferInfo = pointLightZBinsBufferInfo;
	transparencyWriteData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferHandle;
	transparencyWriteData.m_indirectBufferHandle = transparentIndirectBufferHandle;
	transparencyWriteData.m_depthImageHandle = depthImageHandle;
	transparencyWriteData.m_shadowAtlasImageHandle = shadowAtlasImageHandle;
	transparencyWriteData.m_lightImageHandle = lightImageHandle;

	if (renderData.m_transparentBatchSize)
	{
		VKTransparencyWritePass::addToGraph(graph, transparencyWriteData);
	}


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
		VkResult result = vkAcquireNextImageKHR(g_context.m_device, swapChain, std::numeric_limits<uint64_t>::max(), m_renderResources->m_swapChainImageAvailableSemaphores[commonData.m_currentResourceIndex], VK_NULL_HANDLE, &m_swapChainImageIndex);

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
			m_renderResources->m_swapChainImageAvailableSemaphores[commonData.m_currentResourceIndex],
			m_renderResources->m_swapChainRenderFinishedSemaphores[commonData.m_currentResourceIndex]);
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


	FrameGraph::ImageHandle tonemapTargetTextureHandle = g_FXAAEnabled ? finalImageHandle : swapchainTextureHandle;

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


	// FXAA
	VKFXAAPass::Data fxaaPassData;
	fxaaPassData.m_renderResources = m_renderResources.get();
	fxaaPassData.m_pipelineCache = m_pipelineCache.get();
	fxaaPassData.m_descriptorSetCache = m_descriptorSetCache.get();
	fxaaPassData.m_width = m_width;
	fxaaPassData.m_height = m_height;
	fxaaPassData.m_inputImageHandle = tonemapTargetTextureHandle;
	fxaaPassData.m_resultImageHandle = swapchainTextureHandle;

	if (g_FXAAEnabled)
	{
		VKFXAAPass::addToGraph(graph, fxaaPassData);
	}


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
	luminanceDebugPassData.m_colorImageHandle = swapchainTextureHandle;
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
	memoryHeapDebugPassData.m_colorImageHandle = swapchainTextureHandle;

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
		timingInfoStringData[i] = std::to_string(timingInfos[i].m_passTimeWithSync) + "ms "
			//+ std::to_string(timingInfos[i].m_passTime) + "ms / "
			//+ std::to_string(timingInfos[i].m_passTimeWithSync - timingInfos[i].m_passTime) + "ms / "
			+ timingInfos[i].m_passName;
		timingInfoStrings[i].m_chars = timingInfoStringData[i].c_str();
		timingInfoStrings[i].m_positionX = 0;
		timingInfoStrings[i].m_positionY = i * 20;

		totalPassOnly += timingInfos[i].m_passTime;
		totalSyncOnly += timingInfos[i].m_passTimeWithSync - timingInfos[i].m_passTime;
		total += timingInfos[i].m_passTimeWithSync;
	}

	//timingInfoStringData[timingInfoCount] = std::to_string(totalPassOnly) + "ms Total Pass-Only";
	//timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	//timingInfoStrings[timingInfoCount].m_positionX = 0;
	//timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	//++timingInfoCount;
	//timingInfoStringData[timingInfoCount] = std::to_string(totalSyncOnly) + "ms Total Sync-Only";
	//timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	//timingInfoStrings[timingInfoCount].m_positionX = 0;
	//timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	//++timingInfoCount;
	timingInfoStringData[timingInfoCount] = std::to_string(total) + "ms Total";
	timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	timingInfoStrings[timingInfoCount].m_positionX = 0;
	timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	++timingInfoCount;
	//timingInfoStringData[timingInfoCount] = std::to_string(totalSyncOnly / total * 100.0f) + "% Sync of total time";
	//timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	//timingInfoStrings[timingInfoCount].m_positionX = 0;
	//timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	//++timingInfoCount;

	VKTextPass::Data textPassData;
	textPassData.m_renderResources = m_renderResources.get();
	textPassData.m_pipelineCache = m_pipelineCache.get();
	textPassData.m_width = m_width;
	textPassData.m_height = m_height;
	textPassData.m_atlasTextureIndex = m_fontAtlasTextureIndex;
	textPassData.m_stringCount = timingInfoCount;
	textPassData.m_strings = timingInfoStrings;
	textPassData.m_colorImageHandle = swapchainTextureHandle;

	VKTextPass::addToGraph(graph, textPassData);


	// blit to swapchain image
	//VkOffset3D blitSize;
	//blitSize.x = m_width;
	//blitSize.y = m_height;
	//blitSize.z = 1;
	//
	//VkImageBlit imageBlitRegion = {};
	//imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//imageBlitRegion.srcSubresource.layerCount = 1;
	//imageBlitRegion.srcOffsets[1] = blitSize;
	//imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//imageBlitRegion.dstSubresource.layerCount = 1;
	//imageBlitRegion.dstOffsets[1] = blitSize;
	//
	//VKBlitPass::Data finalBlitPassData;
	//finalBlitPassData.m_regionCount = 1;
	//finalBlitPassData.m_regions = &imageBlitRegion;
	//finalBlitPassData.m_filter = VK_FILTER_NEAREST;
	//finalBlitPassData.m_srcImage = tonemapTargetTextureHandle;
	//finalBlitPassData.m_dstImage = swapchainTextureHandle;
	//
	//if (m_swapChain->getImageFormat() != VK_FORMAT_R8G8B8A8_UNORM)
	//{
	//	VKBlitPass::addToGraph(graph, finalBlitPassData, "Final Blit Pass");
	//}


	graph.execute(FrameGraph::ResourceHandle(swapchainTextureHandle), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderResources->m_swapChainRenderFinishedSemaphores[commonData.m_currentResourceIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &m_swapChainImageIndex;


	if (vkQueuePresentKHR(g_context.m_graphicsQueue, &presentInfo) != VK_SUCCESS)
	{
		Utility::fatalExit("Failed to present!", -1);
	}
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

void VEngine::VKRenderer::createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles)
{
	m_meshManager->createSubMeshes(count, subMeshes, handles);
}

void VEngine::VKRenderer::destroySubMeshes(uint32_t count, SubMeshHandle *handles)
{
	m_meshManager->destroySubMeshes(count, handles);
}

void VEngine::VKRenderer::updateTextureData()
{
	const VkDescriptorImageInfo *imageInfos;
	size_t count;
	m_textureLoader->getDescriptorImageInfos(&imageInfos, count);
	m_renderResources->updateTextureArray(imageInfos, count);
}