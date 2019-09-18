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
#include "Pass/ImGuiPass.h"
#include "Module/VKSDSMModule.h"
#include "VKPipelineCache.h"
#include "VKDescriptorSetCache.h"
#include "VKMaterialManager.h"
#include "VKMeshManager.h"
#include "VKResourceDefinitions.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include "RenderGraph.h"
#include "PassRecordContext.h"
#include "RenderPassCache.h"
#include "DeferredObjectDeleter.h"
#include "Graphics/imgui/imgui.h"

VEngine::VKRenderer::VKRenderer(uint32_t width, uint32_t height, void *windowHandle)
{
	g_context.init(static_cast<GLFWwindow *>(windowHandle));

	m_pipelineCache = std::make_unique<VKPipelineCache>();
	m_renderPassCache = std::make_unique<RenderPassCache>();
	m_descriptorSetCache = std::make_unique<VKDescriptorSetCache>();
	m_renderResources = std::make_unique<VKRenderResources>();
	m_deferredObjectDeleter = std::make_unique<DeferredObjectDeleter>();
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
		m_frameGraphs[i] = std::make_unique<RenderGraph>();
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
	auto &graph = *m_frameGraphs[commonData.m_currentResourceIndex];

	// reset per frame resources
	graph.reset();
	m_renderResources->m_mappableUBOBlock[commonData.m_currentResourceIndex]->reset();
	m_renderResources->m_mappableSSBOBlock[commonData.m_currentResourceIndex]->reset();
	m_descriptorSetCache->update(commonData.m_frame, commonData.m_frame - RendererConsts::FRAMES_IN_FLIGHT);
	m_deferredObjectDeleter->update(commonData.m_frame, commonData.m_frame - RendererConsts::FRAMES_IN_FLIGHT);

	// import resources into graph

	ImageViewHandle shadowAtlasImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "Shadow Atlas";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = g_shadowAtlasSize;
		desc.m_height = g_shadowAtlasSize;
		desc.m_format = m_renderResources->m_shadowTexture.getFormat();

		ImageHandle shadowAtlasImageHandle = graph.importImage(desc, m_renderResources->m_shadowTexture.getImage(), &m_renderResources->m_shadowMapQueue, &m_renderResources->m_shadowMapResourceState);
		shadowAtlasImageViewHandle = graph.createImageView({ desc.m_name, shadowAtlasImageHandle, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle taaHistoryImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "TAA History Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_taaHistoryTextures[commonData.m_previousResourceIndex].getFormat();

		ImageHandle taaHistoryImageHandle = graph.importImage(desc, m_renderResources->m_taaHistoryTextures[commonData.m_previousResourceIndex].getImage(), &m_renderResources->m_taaHistoryTextureQueue[commonData.m_previousResourceIndex], &m_renderResources->m_taaHistoryTextureResourceState[commonData.m_previousResourceIndex]);
		taaHistoryImageViewHandle = graph.createImageView({ desc.m_name, taaHistoryImageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle taaResolveImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "TAA Resolve Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_taaHistoryTextures[commonData.m_currentResourceIndex].getFormat();

		ImageHandle taaResolveImageHandle = graph.importImage(desc, m_renderResources->m_taaHistoryTextures[commonData.m_currentResourceIndex].getImage(), &m_renderResources->m_taaHistoryTextureQueue[commonData.m_currentResourceIndex], &m_renderResources->m_taaHistoryTextureResourceState[commonData.m_currentResourceIndex]);
		taaResolveImageViewHandle = graph.createImageView({ desc.m_name, taaResolveImageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle gtaoPreviousImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "GTAO Previous Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_gtaoHistoryTextures[commonData.m_previousResourceIndex].getFormat();

		ImageHandle gtaoPreviousImageHandle = graph.importImage(desc, m_renderResources->m_gtaoHistoryTextures[commonData.m_previousResourceIndex].getImage(), &m_renderResources->m_gtaoHistoryTextureQueue[commonData.m_previousResourceIndex], &m_renderResources->m_gtaoHistoryTextureResourceState[commonData.m_previousResourceIndex]);
		gtaoPreviousImageViewHandle = graph.createImageView({ desc.m_name, gtaoPreviousImageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle gtaoImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "GTAO Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_gtaoHistoryTextures[commonData.m_currentResourceIndex].getFormat();

		ImageHandle gtaoImageHandle = graph.importImage(desc, m_renderResources->m_gtaoHistoryTextures[commonData.m_currentResourceIndex].getImage(), &m_renderResources->m_gtaoHistoryTextureQueue[commonData.m_currentResourceIndex], &m_renderResources->m_gtaoHistoryTextureResourceState[commonData.m_currentResourceIndex]);
		gtaoImageViewHandle = graph.createImageView({ desc.m_name, gtaoImageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	BufferViewHandle avgLuminanceBufferViewHandle = 0;
	{
		BufferDescription desc = {};
		desc.m_name = "Average Luminance Buffer";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = sizeof(float) * RendererConsts::FRAMES_IN_FLIGHT + 4;

		BufferHandle avgLuminanceBufferHandle = graph.importBuffer(desc, m_renderResources->m_avgLuminanceBuffer.getBuffer(), 0, &m_renderResources->m_avgLuminanceBufferQueue, &m_renderResources->m_avgLuminanceBufferResourceState);
		avgLuminanceBufferViewHandle = graph.createBufferView({ desc.m_name, avgLuminanceBufferHandle, 0, desc.m_size });
	}

	// create graph managed resources
	ImageViewHandle finalImageViewHandle = VKResourceDefinitions::createFinalImageViewHandle(graph, m_width, m_height);
	ImageViewHandle depthImageViewHandle = VKResourceDefinitions::createDepthImageViewHandle(graph, m_width, m_height);
	ImageViewHandle uvImageViewHandle = VKResourceDefinitions::createUVImageViewHandle(graph, m_width, m_height);
	ImageViewHandle ddxyLengthImageViewHandle = VKResourceDefinitions::createDerivativesLengthImageViewHandle(graph, m_width, m_height);
	ImageViewHandle ddxyRotMaterialIdImageViewHandle = VKResourceDefinitions::createDerivativesRotMaterialIdImageViewHandle(graph, m_width, m_height);
	ImageViewHandle tangentSpaceImageViewHandle = VKResourceDefinitions::createTangentSpaceImageViewHandle(graph, m_width, m_height);
	ImageViewHandle velocityImageViewHandle = VKResourceDefinitions::createVelocityImageViewHandle(graph, m_width, m_height);
	ImageViewHandle lightImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	ImageViewHandle transparencyAccumImageViewHandle = VKResourceDefinitions::createTransparencyAccumImageViewHandle(graph, m_width, m_height);
	ImageViewHandle transparencyTransmittanceImageViewHandle = VKResourceDefinitions::createTransparencyTransmittanceImageViewHandle(graph, m_width, m_height);
	ImageViewHandle transparencyDeltaImageViewHandle = VKResourceDefinitions::createTransparencyDeltaImageViewHandle(graph, m_width, m_height);
	ImageViewHandle transparencyResultImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	ImageViewHandle gtaoRawImageViewHandle = VKResourceDefinitions::createGTAOImageViewHandle(graph, m_width, m_height);
	ImageViewHandle gtaoSpatiallyFilteredImageViewHandle = VKResourceDefinitions::createGTAOImageViewHandle(graph, m_width, m_height);
	BufferViewHandle pointLightBitMaskBufferViewHandle = VKResourceDefinitions::createPointLightBitMaskBufferViewHandle(graph, m_width, m_height, static_cast<uint32_t>(lightData.m_pointLightData.size()));
	BufferViewHandle luminanceHistogramBufferViewHandle = VKResourceDefinitions::createLuminanceHistogramBufferViewHandle(graph);
	BufferViewHandle opaqueIndirectBufferViewHandle = VKResourceDefinitions::createIndirectBufferViewHandle(graph, renderData.m_opaqueBatchSize);
	BufferViewHandle maskedIndirectBufferViewHandle = VKResourceDefinitions::createIndirectBufferViewHandle(graph, renderData.m_alphaTestedBatchSize);
	BufferViewHandle transparentIndirectBufferViewHandle = VKResourceDefinitions::createIndirectBufferViewHandle(graph, renderData.m_transparentBatchSize);
	BufferViewHandle opaqueShadowIndirectBufferViewHandle = VKResourceDefinitions::createIndirectBufferViewHandle(graph, renderData.m_opaqueShadowBatchSize);
	BufferViewHandle maskedShadowIndirectBufferViewHandle = VKResourceDefinitions::createIndirectBufferViewHandle(graph, renderData.m_alphaTestedShadowBatchSize);

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

	BufferViewHandle shadowDataBufferViewHandle = 0;
	{
		BufferDescription desc = {};
		desc.m_name = "Shadow Data Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = shadowDataBufferInfo.range;

		BufferHandle shadowDataBufferHandle = graph.importBuffer(desc, shadowDataBufferInfo.buffer, shadowDataBufferInfo.offset);
		shadowDataBufferViewHandle = graph.createBufferView({ desc.m_name, shadowDataBufferHandle, 0, desc.m_size });
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

	PassRecordContext passRecordContext{};
	passRecordContext.m_renderResources = m_renderResources.get();
	passRecordContext.m_deferredObjectDeleter = m_deferredObjectDeleter.get();
	passRecordContext.m_renderPassCache = m_renderPassCache.get();
	passRecordContext.m_pipelineCache = m_pipelineCache.get();
	passRecordContext.m_descriptorSetCache = m_descriptorSetCache.get();
	passRecordContext.m_commonRenderData = &commonData;

	// prepare indirect buffers
	VKPrepareIndirectBuffersPass::Data prepareIndirectBuffersPassData;
	prepareIndirectBuffersPassData.m_passRecordContext = &passRecordContext;
	prepareIndirectBuffersPassData.m_opaqueCount = renderData.m_opaqueBatchSize;
	prepareIndirectBuffersPassData.m_maskedCount = renderData.m_alphaTestedBatchSize;
	prepareIndirectBuffersPassData.m_transparentCount = renderData.m_transparentBatchSize;
	prepareIndirectBuffersPassData.m_opaqueShadowCount = renderData.m_opaqueShadowBatchSize;
	prepareIndirectBuffersPassData.m_maskedShadowCount = renderData.m_alphaTestedShadowBatchSize;
	prepareIndirectBuffersPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	prepareIndirectBuffersPassData.m_subMeshDataBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
	prepareIndirectBuffersPassData.m_opaqueIndirectBufferHandle = opaqueIndirectBufferViewHandle;
	prepareIndirectBuffersPassData.m_maskedIndirectBufferHandle = maskedIndirectBufferViewHandle;
	prepareIndirectBuffersPassData.m_transparentIndirectBufferHandle = transparentIndirectBufferViewHandle;
	prepareIndirectBuffersPassData.m_opaqueShadowIndirectBufferHandle = opaqueShadowIndirectBufferViewHandle;
	prepareIndirectBuffersPassData.m_maskedShadowIndirectBufferHandle = maskedShadowIndirectBufferViewHandle;

	if (renderData.m_opaqueBatchSize || renderData.m_alphaTestedBatchSize || renderData.m_opaqueShadowBatchSize || renderData.m_alphaTestedShadowBatchSize)
	{
		VKPrepareIndirectBuffersPass::addToGraph(graph, prepareIndirectBuffersPassData);
	}

	// draw opaque geometry to gbuffer
	VKGeometryPass::Data opaqueGeometryPassData;
	opaqueGeometryPassData.m_passRecordContext = &passRecordContext;
	opaqueGeometryPassData.m_drawCount = renderData.m_opaqueBatchSize;
	opaqueGeometryPassData.m_alphaMasked = false;
	opaqueGeometryPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	opaqueGeometryPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
	opaqueGeometryPassData.m_transformDataBufferInfo = transformDataBufferInfo;
	opaqueGeometryPassData.m_indirectBufferHandle = opaqueIndirectBufferViewHandle;
	opaqueGeometryPassData.m_depthImageHandle = depthImageViewHandle;
	opaqueGeometryPassData.m_uvImageHandle = uvImageViewHandle;
	opaqueGeometryPassData.m_ddxyLengthImageHandle = ddxyLengthImageViewHandle;
	opaqueGeometryPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageViewHandle;
	opaqueGeometryPassData.m_tangentSpaceImageHandle = tangentSpaceImageViewHandle;

	if (renderData.m_opaqueBatchSize)
	{
		VKGeometryPass::addToGraph(graph, opaqueGeometryPassData);
	}


	// draw alpha masked geometry to gbuffer
	VKGeometryPass::Data maskedGeometryPassData = opaqueGeometryPassData;
	maskedGeometryPassData.m_drawCount = renderData.m_alphaTestedBatchSize;
	maskedGeometryPassData.m_alphaMasked = true;
	maskedGeometryPassData.m_indirectBufferHandle = maskedIndirectBufferViewHandle;

	if (renderData.m_alphaTestedBatchSize)
	{
		VKGeometryPass::addToGraph(graph, maskedGeometryPassData);
	}

	// common sdsm
	VKSDSMModule::OutputData sdsmOutputData;
	VKSDSMModule::InputData sdsmInputData;
	sdsmInputData.m_passRecordContext = &passRecordContext;
	sdsmInputData.m_depthImageHandle = depthImageViewHandle;

	VKSDSMModule::addToGraph(graph, sdsmInputData, sdsmOutputData);


	// sdsm shadow matrix
	VKSDSMShadowMatrixPass::Data sdsmShadowMatrixPassData;
	sdsmShadowMatrixPassData.m_passRecordContext = &passRecordContext;
	sdsmShadowMatrixPassData.m_lightView = glm::lookAt(glm::vec3(), -glm::vec3(commonData.m_invViewMatrix * lightData.m_directionalLightData.front().m_direction), glm::vec3(glm::transpose(commonData.m_viewMatrix)[0]));
	sdsmShadowMatrixPassData.m_lightSpaceNear = renderData.m_orthoNearest;
	sdsmShadowMatrixPassData.m_lightSpaceFar = renderData.m_orthoFarthest;
	sdsmShadowMatrixPassData.m_shadowDataBufferHandle = shadowDataBufferViewHandle;
	sdsmShadowMatrixPassData.m_partitionBoundsBufferHandle = sdsmOutputData.m_partitionBoundsBufferHandle;

	VKSDSMShadowMatrixPass::addToGraph(graph, sdsmShadowMatrixPassData);


	// initialize velocity of static objects
	VKVelocityInitializationPass::Data velocityInitializationPassData;
	velocityInitializationPassData.m_passRecordContext = &passRecordContext;
	velocityInitializationPassData.m_depthImageHandle = depthImageViewHandle;
	velocityInitializationPassData.m_velocityImageHandle = velocityImageViewHandle;

	VKVelocityInitializationPass::addToGraph(graph, velocityInitializationPassData);


	// draw shadows
	VKShadowPass::Data opaqueShadowPassData;
	opaqueShadowPassData.m_passRecordContext = &passRecordContext;
	opaqueShadowPassData.m_drawCount = renderData.m_opaqueShadowBatchSize;
	opaqueShadowPassData.m_shadowJobCount = static_cast<uint32_t>(lightData.m_shadowJobs.size());
	opaqueShadowPassData.m_shadowJobs = lightData.m_shadowJobs.data();
	opaqueShadowPassData.m_alphaMasked = false;
	opaqueShadowPassData.m_clear = true;
	opaqueShadowPassData.m_instanceDataBufferInfo = opaqueGeometryPassData.m_instanceDataBufferInfo;
	opaqueShadowPassData.m_materialDataBufferInfo = opaqueGeometryPassData.m_materialDataBufferInfo;
	opaqueShadowPassData.m_transformDataBufferInfo = opaqueGeometryPassData.m_transformDataBufferInfo;
	opaqueShadowPassData.m_shadowDataBufferHandle = shadowDataBufferViewHandle;
	opaqueShadowPassData.m_indirectBufferHandle = opaqueShadowIndirectBufferViewHandle;
	opaqueShadowPassData.m_shadowAtlasImageHandle = shadowAtlasImageViewHandle;

	if (renderData.m_opaqueShadowBatchSize && !lightData.m_shadowJobs.empty())
	{
		VKShadowPass::addToGraph(graph, opaqueShadowPassData);
	}


	// draw masked shadows
	VKShadowPass::Data maskedShadowPassData = opaqueShadowPassData;
	maskedShadowPassData.m_drawCount = renderData.m_alphaTestedShadowBatchSize;
	maskedShadowPassData.m_alphaMasked = true;
	maskedShadowPassData.m_clear = lightData.m_shadowJobs.empty();
	maskedShadowPassData.m_indirectBufferHandle = maskedShadowIndirectBufferViewHandle;

	if (renderData.m_alphaTestedShadowBatchSize && !lightData.m_shadowJobs.empty())
	{
		VKShadowPass::addToGraph(graph, maskedShadowPassData);
	}


	// gtao
	VKGTAOPass::Data gtaoPassData;
	gtaoPassData.m_passRecordContext = &passRecordContext;
	gtaoPassData.m_depthImageHandle = depthImageViewHandle;
	gtaoPassData.m_resultImageHandle = gtaoRawImageViewHandle;

	if (g_ssaoEnabled)
	{
		VKGTAOPass::addToGraph(graph, gtaoPassData);
	}


	// gtao spatial filter
	VKGTAOSpatialFilterPass::Data gtaoPassSpatialFilterPassData;
	gtaoPassSpatialFilterPassData.m_passRecordContext = &passRecordContext;
	gtaoPassSpatialFilterPassData.m_inputImageHandle = gtaoRawImageViewHandle;
	gtaoPassSpatialFilterPassData.m_resultImageHandle = gtaoSpatiallyFilteredImageViewHandle;

	if (g_ssaoEnabled)
	{
		VKGTAOSpatialFilterPass::addToGraph(graph, gtaoPassSpatialFilterPassData);
	}


	// gtao temporal filter
	VKGTAOTemporalFilterPass::Data gtaoPassTemporalFilterPassData;
	gtaoPassTemporalFilterPassData.m_passRecordContext = &passRecordContext;
	gtaoPassTemporalFilterPassData.m_inputImageHandle = gtaoSpatiallyFilteredImageViewHandle;
	gtaoPassTemporalFilterPassData.m_velocityImageHandle = velocityImageViewHandle;
	gtaoPassTemporalFilterPassData.m_previousImageHandle = gtaoPreviousImageViewHandle;
	gtaoPassTemporalFilterPassData.m_resultImageHandle = gtaoImageViewHandle;

	if (g_ssaoEnabled)
	{
		VKGTAOTemporalFilterPass::addToGraph(graph, gtaoPassTemporalFilterPassData);
	}


	// cull lights to tiles
	VKRasterTilingPass::Data rasterTilingPassData;
	rasterTilingPassData.m_passRecordContext = &passRecordContext;
	rasterTilingPassData.m_lightData = &lightData;
	rasterTilingPassData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferViewHandle;

	if (!lightData.m_pointLightData.empty())
	{
		VKRasterTilingPass::addToGraph(graph, rasterTilingPassData);
	}


	// light gbuffer
	VKDirectLightingPass::Data lightingPassData;
	lightingPassData.m_passRecordContext = &passRecordContext;
	lightingPassData.m_ssao = g_ssaoEnabled;
	lightingPassData.m_directionalLightDataBufferInfo = directionalLightDataBufferInfo;
	lightingPassData.m_pointLightDataBufferInfo = pointLightDataBufferInfo;
	lightingPassData.m_pointLightZBinsBufferInfo = pointLightZBinsBufferInfo;
	lightingPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
	lightingPassData.m_shadowDataBufferHandle = shadowDataBufferViewHandle;
	lightingPassData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferViewHandle;
	lightingPassData.m_depthImageHandle = depthImageViewHandle;
	lightingPassData.m_uvImageHandle = uvImageViewHandle;
	lightingPassData.m_ddxyLengthImageHandle = ddxyLengthImageViewHandle;
	lightingPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageViewHandle;
	lightingPassData.m_tangentSpaceImageHandle = tangentSpaceImageViewHandle;
	lightingPassData.m_shadowAtlasImageHandle = shadowAtlasImageViewHandle;
	lightingPassData.m_occlusionImageHandle = gtaoImageViewHandle;
	lightingPassData.m_resultImageHandle = lightImageViewHandle;
	lightingPassData.m_shadowSplitsBufferHandle = sdsmOutputData.m_splitsBufferHandle;

	VKDirectLightingPass::addToGraph(graph, lightingPassData);


	// calculate luminance histograms
	VKLuminanceHistogramPass::Data luminanceHistogramPassData;
	luminanceHistogramPassData.m_passRecordContext = &passRecordContext;
	luminanceHistogramPassData.m_logMin = -10.0f;
	luminanceHistogramPassData.m_logMax = 17.0f;
	luminanceHistogramPassData.m_lightImageHandle = lightImageViewHandle;
	luminanceHistogramPassData.m_luminanceHistogramBufferHandle = luminanceHistogramBufferViewHandle;

	VKLuminanceHistogramPass::addToGraph(graph, luminanceHistogramPassData);


	// calculate avg luminance
	VKLuminanceHistogramAveragePass::Data luminanceHistogramAvgPassData;
	luminanceHistogramAvgPassData.m_passRecordContext = &passRecordContext;
	luminanceHistogramAvgPassData.m_logMin = -10.0f;
	luminanceHistogramAvgPassData.m_logMax = 17.0f;
	luminanceHistogramAvgPassData.m_luminanceHistogramBufferHandle = luminanceHistogramBufferViewHandle;
	luminanceHistogramAvgPassData.m_avgLuminanceBufferHandle = avgLuminanceBufferViewHandle;

	VKLuminanceHistogramAveragePass::addToGraph(graph, luminanceHistogramAvgPassData);


	VkSwapchainKHR swapChain = m_swapChain->get();

	// get swapchain image
	ImageViewHandle swapchainImageViewHandle = 0;
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

		ImageDescription desc{};
		desc.m_name = "Swapchain Image";
		desc.m_concurrent = true;
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_swapChain->getImageFormat();
		


		auto imageHandle = graph.importImage(desc, m_swapChain->getImage(m_swapChainImageIndex));
		swapchainImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}


	// taa resolve
	VKTAAResolvePass::Data taaResolvePassData;
	taaResolvePassData.m_passRecordContext = &passRecordContext;
	taaResolvePassData.m_jitterOffsetX = commonData.m_jitteredProjectionMatrix[2][0];
	taaResolvePassData.m_jitterOffsetY = commonData.m_jitteredProjectionMatrix[2][1];
	taaResolvePassData.m_depthImageHandle = depthImageViewHandle;
	taaResolvePassData.m_velocityImageHandle = velocityImageViewHandle;
	taaResolvePassData.m_taaHistoryImageHandle = taaHistoryImageViewHandle;
	taaResolvePassData.m_lightImageHandle = lightImageViewHandle;
	taaResolvePassData.m_taaResolveImageHandle = taaResolveImageViewHandle;

	if (g_TAAEnabled)
	{
		VKTAAResolvePass::addToGraph(graph, taaResolvePassData);

		lightImageViewHandle = taaResolveImageViewHandle;
	}


	ImageViewHandle tonemapTargetTextureHandle = g_FXAAEnabled ? finalImageViewHandle : swapchainImageViewHandle;

	// tonemap
	VKTonemapPass::Data tonemapPassData;
	tonemapPassData.m_passRecordContext = &passRecordContext;
	tonemapPassData.m_srcImageHandle = lightImageViewHandle;
	tonemapPassData.m_dstImageHandle = tonemapTargetTextureHandle;
	tonemapPassData.m_avgLuminanceBufferHandle = avgLuminanceBufferViewHandle;

	VKTonemapPass::addToGraph(graph, tonemapPassData);


	// FXAA
	VKFXAAPass::Data fxaaPassData;
	fxaaPassData.m_passRecordContext = &passRecordContext;
	fxaaPassData.m_inputImageHandle = tonemapTargetTextureHandle;
	fxaaPassData.m_resultImageHandle = swapchainImageViewHandle;

	if (g_FXAAEnabled)
	{
		VKFXAAPass::addToGraph(graph, fxaaPassData);
	}


	// ImGui
	ImGuiPass::Data imGuiPassData;
	imGuiPassData.m_passRecordContext = &passRecordContext;
	imGuiPassData.m_imGuiDrawData = ImGui::GetDrawData();
	imGuiPassData.m_resultImageViewHandle = swapchainImageViewHandle;

	ImGuiPass::addToGraph(graph, imGuiPassData);


	// text pass
	//PassTimingInfo timingInfos[128];
	//size_t timingInfoCount;

	//graph.getTimingInfo(timingInfoCount, timingInfos);

	//VKTextPass::String timingInfoStrings[128];
	//std::string timingInfoStringData[128];

	//float totalPassOnly = 0.0f;
	//float totalSyncOnly = 0.0f;
	//float total = 0.0f;

	//for (size_t i = 0; i < timingInfoCount; ++i)
	//{
	//	timingInfoStringData[i] = std::to_string(timingInfos[i].m_passTimeWithSync) + "ms "
	//		//+ std::to_string(timingInfos[i].m_passTime) + "ms / "
	//		//+ std::to_string(timingInfos[i].m_passTimeWithSync - timingInfos[i].m_passTime) + "ms / "
	//		+ timingInfos[i].m_passName;
	//	timingInfoStrings[i].m_chars = timingInfoStringData[i].c_str();
	//	timingInfoStrings[i].m_positionX = 0;
	//	timingInfoStrings[i].m_positionY = i * 20;

	//	totalPassOnly += timingInfos[i].m_passTime;
	//	totalSyncOnly += timingInfos[i].m_passTimeWithSync - timingInfos[i].m_passTime;
	//	total += timingInfos[i].m_passTimeWithSync;
	//}

	////timingInfoStringData[timingInfoCount] = std::to_string(totalPassOnly) + "ms Total Pass-Only";
	////timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	////timingInfoStrings[timingInfoCount].m_positionX = 0;
	////timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	////++timingInfoCount;
	////timingInfoStringData[timingInfoCount] = std::to_string(totalSyncOnly) + "ms Total Sync-Only";
	////timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	////timingInfoStrings[timingInfoCount].m_positionX = 0;
	////timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	////++timingInfoCount;
	//timingInfoStringData[timingInfoCount] = std::to_string(total) + "ms Total";
	//timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	//timingInfoStrings[timingInfoCount].m_positionX = 0;
	//timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	//++timingInfoCount;
	////timingInfoStringData[timingInfoCount] = std::to_string(totalSyncOnly / total * 100.0f) + "% Sync of total time";
	////timingInfoStrings[timingInfoCount].m_chars = timingInfoStringData[timingInfoCount].c_str();
	////timingInfoStrings[timingInfoCount].m_positionX = 0;
	////timingInfoStrings[timingInfoCount].m_positionY = timingInfoCount * 20;
	////++timingInfoCount;

	//VKTextPass::Data textPassData;
	//textPassData.m_renderResources = m_renderResources.get();
	//textPassData.m_pipelineCache = m_pipelineCache.get();
	//textPassData.m_width = m_width;
	//textPassData.m_height = m_height;
	//textPassData.m_atlasTextureIndex = m_fontAtlasTextureIndex;
	//textPassData.m_stringCount = timingInfoCount;
	//textPassData.m_strings = timingInfoStrings;
	//textPassData.m_colorImageHandle = swapchainTextureHandle;

	//VKTextPass::addToGraph(graph, textPassData);

	uint32_t semaphoreCount;
	VkSemaphore *semaphores;
	graph.execute(ResourceViewHandle(swapchainImageViewHandle), m_renderResources->m_swapChainImageAvailableSemaphores[commonData.m_currentResourceIndex], &semaphoreCount, &semaphores, ResourceState::PRESENT_IMAGE, QueueType::GRAPHICS);

	VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	presentInfo.waitSemaphoreCount = semaphoreCount;
	presentInfo.pWaitSemaphores = semaphores;
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