#include "Renderer.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "RenderResources.h"
#include "Utility/Utility.h"
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"
#include "TextureLoader.h"
#include "GlobalVar.h"
#include "Pass/IntegrateBrdfPass2.h"
#include "Pass/GeometryPass2.h"
#include "Pass/ShadowPass2.h"
#include "Pass/RasterTilingPass2.h"
#include "Pass/LuminanceHistogramPass2.h"
#include "Pass/LuminanceHistogramReduceAveragePass2.h"
#include "Pass/TonemapPass2.h"
#include "Pass/VelocityInitializationPass2.h"
#include "Pass/FXAAPass2.h"
#include "Pass/LightingPass2.h"
#include "Pass/DeferredShadowsPass2.h"
#include "Pass/ImGuiPass2.h"
#include "Pass/ReadBackCopyPass2.h"
#include "Pass/HiZPyramidPass2.h"
#include "Pass/BuildIndexBufferPass2.h"
#include "Pass/SharpenFfxCasPass2.h"
#include "Pass/TAAPass2.h"
#include "Module/GTAOModule2.h"
#include "Module/SSRModule2.h"
#include "Module/BloomModule2.h"
#include "PipelineCache.h"
#include "DescriptorSetCache.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include "RenderGraph2.h"
#include "PassRecordContext2.h"
#include "Graphics/imgui/imgui.h"
#include "Graphics/ViewRenderList.h"
#include "ResourceDefinitions.h"

using namespace VEngine::gal;

extern uint32_t g_debugVoxelCascadeIndex;
extern uint32_t g_giVoxelDebugMode;
extern uint32_t g_allocatedBricks;
extern bool g_forceVoxelization;
extern bool g_voxelizeOnDemand;

VEngine::Renderer::Renderer(uint32_t width, uint32_t height, void *windowHandle)
	:m_graphicsDevice(GraphicsDevice::create(windowHandle, true, GraphicsBackendType::VULKAN))
{
	m_graphicsDevice->createSwapChain(m_graphicsDevice->getGraphicsQueue(), width, height, &m_swapChain);
	m_graphicsDevice->createSemaphore(0, &m_semaphores[0]);
	m_graphicsDevice->createSemaphore(0, &m_semaphores[1]);
	m_graphicsDevice->createSemaphore(0, &m_semaphores[2]);

	m_width = m_swapChain->getExtent().m_width;
	m_height = m_swapChain->getExtent().m_height;

	m_renderResources = std::make_unique<RenderResources>(m_graphicsDevice);
	m_renderResources->init(m_width, m_height);

	m_pipelineCache = std::make_unique<PipelineCache>(m_graphicsDevice);
	m_descriptorSetCache = std::make_unique<DescriptorSetCache2>(m_graphicsDevice);
	m_textureLoader = std::make_unique<TextureLoader>(m_graphicsDevice, m_renderResources->m_stagingBuffer);
	m_materialManager = std::make_unique<MaterialManager>(m_graphicsDevice, m_renderResources->m_stagingBuffer, m_renderResources->m_materialBuffer);
	m_meshManager = std::make_unique<MeshManager>(m_graphicsDevice, m_renderResources->m_stagingBuffer, m_renderResources->m_vertexBuffer, m_renderResources->m_indexBuffer, m_renderResources->m_subMeshDataInfoBuffer, m_renderResources->m_subMeshBoundingBoxBuffer);
	

	m_fontAtlasTextureIndex = m_textureLoader->load("Resources/Textures/fontConsolas.dds");
	m_blueNoiseTextureIndex = m_textureLoader->load("Resources/Textures/blue_noise_LDR_RGBA_0.dds");

	updateTextureData();

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_frameGraphs[i] = std::make_unique<rg::RenderGraph2>(m_graphicsDevice, m_semaphores, (uint64_t **)&m_semaphoreValues);
	}

	m_gtaoModule = std::make_unique<GTAOModule2>(m_graphicsDevice, m_width, m_height);
	m_ssrModule = std::make_unique<SSRModule2>(m_graphicsDevice, m_width, m_height);
}

VEngine::Renderer::~Renderer()
{
	m_graphicsDevice->waitIdle();
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_frameGraphs[i].reset();
	}
	m_pipelineCache.reset();
	m_descriptorSetCache.reset();
	m_textureLoader.reset();
	m_materialManager.reset();
	m_meshManager.reset();
	m_gtaoModule.reset();
	m_ssrModule.reset();
	m_renderResources.reset();

	m_graphicsDevice->destroySemaphore(m_semaphores[0]);
	m_graphicsDevice->destroySemaphore(m_semaphores[1]);
	m_graphicsDevice->destroySemaphore(m_semaphores[2]);
	m_graphicsDevice->destroySwapChain();
	GraphicsDevice::destroy(m_graphicsDevice);
}

void VEngine::Renderer::render(const CommonRenderData &commonData, const RenderData &renderData, const LightData &lightData)
{
	auto swapChainRecreationExtent = m_swapChain->getRecreationExtent();
	if (m_width == 0 || m_height == 0 || swapChainRecreationExtent.m_width == 0 || swapChainRecreationExtent.m_height == 0)
	{
		return;
	}
	auto &graph = *m_frameGraphs[commonData.m_curResIdx];

	// reset per frame resources
	graph.reset();
	m_renderResources->m_mappableUBOBlock[commonData.m_curResIdx]->reset();
	m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->reset();
	m_descriptorSetCache->update(commonData.m_frame, commonData.m_frame - RendererConsts::FRAMES_IN_FLIGHT);

	// read back luminance histogram
	{
		auto *buffer = m_renderResources->m_luminanceHistogramReadBackBuffers[commonData.m_curResIdx];
		uint32_t *data;
		buffer->map((void **)&data);

		MemoryRange range{ 0, sizeof(m_luminanceHistogram) };
		buffer->invalidate(1, &range);
		memcpy(m_luminanceHistogram, data, sizeof(m_luminanceHistogram));

		buffer->unmap();
	}

	// get timing data
	graph.getTimingInfo(&m_passTimingCount, &m_passTimingData);


	// import resources into graph

	rg::ImageViewHandle depthImageViewHandle = 0;
	{
		rg::ImageHandle imageHandle = graph.importImage(m_renderResources->m_depthImages[commonData.m_curResIdx], "Depth Image", false, {}, &m_renderResources->m_depthImageState[commonData.m_curResIdx]);
		depthImageViewHandle = graph.createImageView({ "Depth Image", imageHandle, { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle prevDepthImageViewHandle = 0;
	{
		rg::ImageHandle imageHandle = graph.importImage(m_renderResources->m_depthImages[commonData.m_prevResIdx], "Prev Depth Image", false, {}, &m_renderResources->m_depthImageState[commonData.m_prevResIdx]);
		prevDepthImageViewHandle = graph.createImageView({ "Prev Depth Image", imageHandle, { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle lightImageViewHandle = 0;
	{
		rg::ImageHandle imageHandle = graph.importImage(m_renderResources->m_lightImages[commonData.m_curResIdx], "Light Image", false, {}, m_renderResources->m_lightImageState[commonData.m_curResIdx]);
		lightImageViewHandle = graph.createImageView({ "Light Image", imageHandle, {  0, 1, 0, 1 } });
	}

	rg::ImageHandle prevLightImageHandle = 0;
	rg::ImageViewHandle prevLightImageViewHandle = 0;
	{
		prevLightImageHandle = graph.importImage(m_renderResources->m_lightImages[commonData.m_prevResIdx], "Prev Light Image", false, {}, m_renderResources->m_lightImageState[commonData.m_prevResIdx]);
		prevLightImageViewHandle = graph.createImageView({ "Prev Light Image", prevLightImageHandle, { 0, m_renderResources->m_lightImages[commonData.m_prevResIdx]->getDescription().m_levels, 0, 1 } });
	}

	rg::ImageViewHandle taaHistoryImageViewHandle = 0;
	{
		rg::ImageHandle taaHistoryImageHandle = graph.importImage(m_renderResources->m_taaHistoryTextures[commonData.m_prevResIdx], "TAA History Image", false, {}, &m_renderResources->m_taaHistoryTextureState[commonData.m_prevResIdx]);
		taaHistoryImageViewHandle = graph.createImageView({ "TAA History Image", taaHistoryImageHandle, { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle taaResolveImageViewHandle = 0;
	{
		rg::ImageHandle taaResolveImageHandle = graph.importImage(m_renderResources->m_taaHistoryTextures[commonData.m_curResIdx], "TAA Resolve Texture", false, {}, &m_renderResources->m_taaHistoryTextureState[commonData.m_curResIdx]);
		taaResolveImageViewHandle = graph.createImageView({ "TAA Resolve Texture", taaResolveImageHandle, { 0, 1, 0, 1 } });
	}

	rg::BufferViewHandle avgLuminanceBufferViewHandle = 0;
	{
		rg::BufferHandle avgLuminanceBufferHandle = graph.importBuffer(m_renderResources->m_avgLuminanceBuffer, "Average Luminance Buffer", false, {}, &m_renderResources->m_avgLuminanceBufferState);
		avgLuminanceBufferViewHandle = graph.createBufferView({ "Average Luminance Buffer", avgLuminanceBufferHandle, 0, m_renderResources->m_avgLuminanceBuffer->getDescription().m_size });
	}

	rg::ImageViewHandle brdfLUTImageViewHandle = 0;
	{
		rg::ImageHandle imageHandle = graph.importImage(m_renderResources->m_brdfLUT, "BRDF LUT Image", false, {}, &m_renderResources->m_brdfLutImageState);
		brdfLUTImageViewHandle = graph.createImageView({ "BRDF LUT Image", imageHandle, { 0, 1, 0, 1 } });
	}

	// create graph managed resources

	rg::ImageViewHandle albedoImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "Albedo Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R8G8B8A8_UNORM;

		albedoImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle normalImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "Normals Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R16G16B16A16_SFLOAT;

		normalImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle ssrRayHitPdfImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "SSR RayHit/PDF Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R16G16B16A16_SFLOAT;

		ssrRayHitPdfImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}


	rg::ImageViewHandle ssrMaskImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "SSR Mask Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R8_UNORM;

		ssrMaskImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}

	rg::ImageHandle deferredShadowsImageHandle;
	rg::ImageViewHandle deferredShadowsImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "Deferred Shadows Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = static_cast<uint32_t>(lightData.m_directionalLightData.size());
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R8_UNORM;

		deferredShadowsImageHandle = graph.createImage(desc);
		deferredShadowsImageViewHandle = graph.createImageView({ desc.m_name, deferredShadowsImageHandle, { 0, 1, 0, desc.m_layers }, ImageViewType::_2D_ARRAY });
	}

	rg::ImageViewHandle indirectDiffuseImageViewHandle = ResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	rg::ImageViewHandle indirectSpecularImageViewHandle = ResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	rg::ImageViewHandle finalImageViewHandle = ResourceDefinitions::createFinalImageViewHandle(graph, m_width, m_height);
	rg::ImageViewHandle finalImageViewHandle2 = ResourceDefinitions::createFinalImageViewHandle(graph, m_width, m_height);
	rg::ImageViewHandle uvImageViewHandle = ResourceDefinitions::createUVImageViewHandle(graph, m_width, m_height);
	rg::ImageViewHandle ddxyLengthImageViewHandle = ResourceDefinitions::createDerivativesLengthImageViewHandle(graph, m_width, m_height);
	rg::ImageViewHandle ddxyRotMaterialIdImageViewHandle = ResourceDefinitions::createDerivativesRotMaterialIdImageViewHandle(graph, m_width, m_height);
	rg::ImageViewHandle tangentSpaceImageViewHandle = ResourceDefinitions::createTangentSpaceImageViewHandle(graph, m_width, m_height);
	rg::ImageViewHandle velocityImageViewHandle = ResourceDefinitions::createVelocityImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle lightImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyAccumImageViewHandle = VKResourceDefinitions::createTransparencyAccumImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyTransmittanceImageViewHandle = VKResourceDefinitions::createTransparencyTransmittanceImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyDeltaImageViewHandle = VKResourceDefinitions::createTransparencyDeltaImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyResultImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle deferredShadowsImageViewHandle = VKResourceDefinitions::createDeferredShadowsImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle reprojectedDepthUintImageViewHandle = VKResourceDefinitions::createReprojectedDepthUintImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle reprojectedDepthImageViewHandle = VKResourceDefinitions::createReprojectedDepthImageViewHandle(graph, m_width, m_height);
	rg::BufferViewHandle pointLightBitMaskBufferViewHandle = ResourceDefinitions::createPointLightBitMaskBufferViewHandle(graph, m_width, m_height, static_cast<uint32_t>(lightData.m_pointLightData.size()));
	rg::BufferViewHandle luminanceHistogramBufferViewHandle = ResourceDefinitions::createLuminanceHistogramBufferViewHandle(graph);
	//BufferViewHandle indirectBufferViewHandle = VKResourceDefinitions::createIndirectBufferViewHandle(graph, renderData.m_subMeshInstanceDataCount);
	//BufferViewHandle visibilityBufferViewHandle = VKResourceDefinitions::createOcclusionCullingVisibilityBufferViewHandle(graph, renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount);
	//BufferViewHandle drawCountsBufferViewHandle = VKResourceDefinitions::createIndirectDrawCountsBufferViewHandle(graph, renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount);


	// transform data write
	DescriptorBufferInfo transformDataBufferInfo{ nullptr, 0, std::max(renderData.m_transformDataCount * sizeof(glm::mat4), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(transformDataBufferInfo.m_range, transformDataBufferInfo.m_offset, transformDataBufferInfo.m_buffer, bufferPtr);
		if (renderData.m_transformDataCount)
		{
			memcpy(bufferPtr, renderData.m_transformData, renderData.m_transformDataCount * sizeof(glm::mat4));
		}
	}

	// directional light data write
	DescriptorBufferInfo directionalLightDataBufferInfo{ nullptr, 0, std::max(lightData.m_directionalLightData.size() * sizeof(DirectionalLightData), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(directionalLightDataBufferInfo.m_range, directionalLightDataBufferInfo.m_offset, directionalLightDataBufferInfo.m_buffer, bufferPtr);
		if (!lightData.m_directionalLightData.empty())
		{
			memcpy(bufferPtr, lightData.m_directionalLightData.data(), lightData.m_directionalLightData.size() * sizeof(DirectionalLightData));
		}
	}

	// point light data write
	DescriptorBufferInfo pointLightDataBufferInfo{ nullptr, 0, std::max(lightData.m_pointLightData.size() * sizeof(PointLightData), size_t(1)) };
	DescriptorBufferInfo pointLightZBinsBufferInfo{ nullptr, 0, std::max(lightData.m_zBins.size() * sizeof(uint32_t), size_t(1)) };
	{
		uint8_t *dataBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(pointLightDataBufferInfo.m_range, pointLightDataBufferInfo.m_offset, pointLightDataBufferInfo.m_buffer, dataBufferPtr);
		uint8_t *zBinsBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(pointLightZBinsBufferInfo.m_range, pointLightZBinsBufferInfo.m_offset, pointLightZBinsBufferInfo.m_buffer, zBinsBufferPtr);
		if (!lightData.m_pointLightData.empty())
		{
			memcpy(dataBufferPtr, lightData.m_pointLightData.data(), lightData.m_pointLightData.size() * sizeof(PointLightData));
			memcpy(zBinsBufferPtr, lightData.m_zBins.data(), lightData.m_zBins.size() * sizeof(uint32_t));
		}
	}

	// shadow matrices write
	DescriptorBufferInfo shadowMatricesBufferInfo{ nullptr, 0, std::max(renderData.m_shadowMatrixCount * sizeof(glm::mat4), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(shadowMatricesBufferInfo.m_range, shadowMatricesBufferInfo.m_offset, shadowMatricesBufferInfo.m_buffer, bufferPtr);
		if (renderData.m_shadowMatrixCount)
		{
			memcpy(bufferPtr, renderData.m_shadowMatrices, renderData.m_shadowMatrixCount * sizeof(glm::mat4));
		}
	}

	// shadow texel sized write
	DescriptorBufferInfo shadowCascadeParamsBufferInfo{ nullptr, 0, std::max(renderData.m_shadowMatrixCount * sizeof(glm::vec4), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(shadowCascadeParamsBufferInfo.m_range, shadowCascadeParamsBufferInfo.m_offset, shadowCascadeParamsBufferInfo.m_buffer, bufferPtr);
		if (renderData.m_shadowMatrixCount)
		{
			memcpy(bufferPtr, renderData.m_shadowCascadeParams, renderData.m_shadowMatrixCount * sizeof(glm::vec4));
		}
	}

	// instance data write
	std::vector<SubMeshInstanceData> sortedInstanceData(renderData.m_subMeshInstanceDataCount);
	DescriptorBufferInfo instanceDataBufferInfo{ nullptr, 0, std::max(renderData.m_subMeshInstanceDataCount * sizeof(SubMeshInstanceData), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(instanceDataBufferInfo.m_range, instanceDataBufferInfo.m_offset, instanceDataBufferInfo.m_buffer, bufferPtr);

		// copy instance data to gpu memory and sort it according to the sorted drawCallKeys list
		SubMeshInstanceData *instanceData = (SubMeshInstanceData *)bufferPtr;
		for (size_t i = 0; i < renderData.m_subMeshInstanceDataCount; ++i)
		{
			instanceData[i] = sortedInstanceData[i] = renderData.m_subMeshInstanceData[static_cast<uint32_t>(renderData.m_drawCallKeys[i] & 0xFFFFFFFF)];
		}
	}

	PassRecordContext2 passRecordContext{};
	passRecordContext.m_renderResources = m_renderResources.get();
	passRecordContext.m_pipelineCache = m_pipelineCache.get();
	passRecordContext.m_descriptorSetCache = m_descriptorSetCache.get();
	passRecordContext.m_commonRenderData = &commonData;

	if (commonData.m_frame == 0)
	{
		IntegrateBrdfPass2::Data integrateBrdfPassData;
		integrateBrdfPassData.m_passRecordContext = &passRecordContext;
		integrateBrdfPassData.m_resultImageViewHandle = brdfLUTImageViewHandle;

		IntegrateBrdfPass2::addToGraph(graph, integrateBrdfPassData);
	}

	// Hi-Z furthest depth pyramid
	HiZPyramidPass2::OutData hiZMinPyramidPassOutData;
	HiZPyramidPass2::Data hiZMinPyramidPassData;
	hiZMinPyramidPassData.m_passRecordContext = &passRecordContext;
	hiZMinPyramidPassData.m_inputImageViewHandle = prevDepthImageViewHandle;
	hiZMinPyramidPassData.m_maxReduction = false;
	hiZMinPyramidPassData.m_copyFirstLevel = false;
	hiZMinPyramidPassData.m_forceExecution = true;

	HiZPyramidPass2::addToGraph(graph, hiZMinPyramidPassData, hiZMinPyramidPassOutData);

	rg::ImageViewHandle depthPyramidImageViewHandle = hiZMinPyramidPassOutData.m_resultImageViewHandle;


	// opaque geometry
	if (renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount)
	{
		// draw opaque geometry to gbuffer
		GeometryPass2::Data opaqueGeometryPassData;
		opaqueGeometryPassData.m_passRecordContext = &passRecordContext;
		opaqueGeometryPassData.m_alphaMasked = false;
		opaqueGeometryPassData.m_instanceDataCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount;
		opaqueGeometryPassData.m_instanceDataOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueOffset;
		opaqueGeometryPassData.m_instanceData = sortedInstanceData.data();
		opaqueGeometryPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
		//opaqueGeometryPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
		opaqueGeometryPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
		opaqueGeometryPassData.m_transformDataBufferInfo = transformDataBufferInfo;
		//opaqueGeometryPassData.m_indirectBufferHandle = opaqueIndirectDrawBufferViewHandle;
		opaqueGeometryPassData.m_depthImageHandle = depthImageViewHandle;
		opaqueGeometryPassData.m_uvImageHandle = uvImageViewHandle;
		opaqueGeometryPassData.m_ddxyLengthImageHandle = ddxyLengthImageViewHandle;
		opaqueGeometryPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageViewHandle;
		opaqueGeometryPassData.m_tangentSpaceImageHandle = tangentSpaceImageViewHandle;
		//opaqueGeometryPassData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
		//opaqueGeometryPassData.m_indicesBufferHandle = opaqueFilteredIndicesBufferViewHandle;

		GeometryPass2::addToGraph(graph, opaqueGeometryPassData);
	}

	// alpha masked geometry
	if (renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedCount)
	{
		// draw alpha masked geometry to gbuffer
		GeometryPass2::Data maskedGeometryPassData;
		maskedGeometryPassData.m_passRecordContext = &passRecordContext;
		maskedGeometryPassData.m_alphaMasked = true;
		maskedGeometryPassData.m_instanceDataCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedCount;
		maskedGeometryPassData.m_instanceDataOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedOffset;
		maskedGeometryPassData.m_instanceData = sortedInstanceData.data();
		maskedGeometryPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
		//maskedGeometryPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
		//maskedGeometryPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
		maskedGeometryPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
		maskedGeometryPassData.m_transformDataBufferInfo = transformDataBufferInfo;
		//maskedGeometryPassData.m_indirectBufferHandle = indirectDrawBufferViewHandle;
		maskedGeometryPassData.m_depthImageHandle = depthImageViewHandle;
		maskedGeometryPassData.m_uvImageHandle = uvImageViewHandle;
		maskedGeometryPassData.m_ddxyLengthImageHandle = ddxyLengthImageViewHandle;
		maskedGeometryPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageViewHandle;
		maskedGeometryPassData.m_tangentSpaceImageHandle = tangentSpaceImageViewHandle;
		//maskedGeometryPassData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
		//maskedGeometryPassData.m_indicesBufferHandle = filteredIndicesBufferViewHandle;

		GeometryPass2::addToGraph(graph, maskedGeometryPassData);
	}

	// initialize velocity of static objects
	VelocityInitializationPass2::Data velocityInitializationPassData;
	velocityInitializationPassData.m_passRecordContext = &passRecordContext;
	velocityInitializationPassData.m_depthImageHandle = depthImageViewHandle;
	velocityInitializationPassData.m_velocityImageHandle = velocityImageViewHandle;

	VelocityInitializationPass2::addToGraph(graph, velocityInitializationPassData);

	rg::ImageViewHandle shadowImageViewHandle = 0;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "Shadow Cascades Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = 2048;
		desc.m_height = 2048;
		desc.m_layers = glm::max(renderData.m_shadowCascadeViewRenderListCount, 1u);
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::D16_UNORM;

		rg::ImageHandle shadowImageHandle = graph.createImage(desc);
		shadowImageViewHandle = graph.createImageView({ desc.m_name, shadowImageHandle, { 0, 1, 0, desc.m_layers }, ImageViewType::_2D_ARRAY });

		for (uint32_t i = 0; i < renderData.m_shadowCascadeViewRenderListCount; ++i)
		{
			rg::ImageViewHandle shadowLayer = graph.createImageView({ desc.m_name, shadowImageHandle, { 0, 1, i, 1 } });

			const auto &drawList = renderData.m_renderLists[renderData.m_shadowCascadeViewRenderListOffset + i];

			// draw shadows
			if (drawList.m_opaqueCount)
			{
				ShadowPass2::Data opaqueShadowPassData;
				opaqueShadowPassData.m_passRecordContext = &passRecordContext;
				opaqueShadowPassData.m_shadowMapSize = 2048;
				opaqueShadowPassData.m_shadowMatrix = renderData.m_shadowMatrices[i];
				opaqueShadowPassData.m_alphaMasked = false;
				opaqueShadowPassData.m_clear = true;
				opaqueShadowPassData.m_instanceDataCount = drawList.m_opaqueCount;
				opaqueShadowPassData.m_instanceDataOffset = drawList.m_opaqueOffset;
				opaqueShadowPassData.m_instanceData = sortedInstanceData.data();
				opaqueShadowPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
				//opaqueShadowPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
				opaqueShadowPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
				opaqueShadowPassData.m_transformDataBufferInfo = transformDataBufferInfo;
				//opaqueShadowPassData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
				//opaqueShadowPassData.m_indicesBufferHandle = filteredIndicesBufferViewHandle;
				//opaqueShadowPassData.m_indirectBufferHandle = indirectDrawBufferViewHandle;
				opaqueShadowPassData.m_shadowImageHandle = shadowLayer;

				ShadowPass2::addToGraph(graph, opaqueShadowPassData);
			}



			// draw masked shadows
			if (drawList.m_maskedCount)
			{
				ShadowPass2::Data maskedShadowPassData;
				maskedShadowPassData.m_passRecordContext = &passRecordContext;
				maskedShadowPassData.m_shadowMapSize = 2048;
				maskedShadowPassData.m_shadowMatrix = renderData.m_shadowMatrices[i];
				maskedShadowPassData.m_alphaMasked = true;
				maskedShadowPassData.m_clear = drawList.m_opaqueCount == 0;
				maskedShadowPassData.m_instanceDataCount = drawList.m_maskedCount;
				maskedShadowPassData.m_instanceDataOffset = drawList.m_maskedOffset;
				maskedShadowPassData.m_instanceData = sortedInstanceData.data();
				maskedShadowPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
				//maskedShadowPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
				maskedShadowPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };;
				maskedShadowPassData.m_transformDataBufferInfo = transformDataBufferInfo;
				//maskedShadowPassData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
				//maskedShadowPassData.m_indicesBufferHandle = filteredIndicesBufferViewHandle;
				//maskedShadowPassData.m_indirectBufferHandle = indirectDrawBufferViewHandle;
				maskedShadowPassData.m_shadowImageHandle = shadowLayer;

				ShadowPass2::addToGraph(graph, maskedShadowPassData);
			}
		}
	}


	// deferred shadows
	DeferredShadowsPass2::Data deferredShadowsPassData;
	deferredShadowsPassData.m_passRecordContext = &passRecordContext;
	deferredShadowsPassData.m_lightDataCount = static_cast<uint32_t>(lightData.m_directionalLightData.size());
	deferredShadowsPassData.m_lightData = lightData.m_directionalLightData.data();
	deferredShadowsPassData.m_resultImageHandle = deferredShadowsImageHandle;
	deferredShadowsPassData.m_depthImageViewHandle = depthImageViewHandle;
	deferredShadowsPassData.m_tangentSpaceImageViewHandle = tangentSpaceImageViewHandle;
	deferredShadowsPassData.m_shadowImageViewHandle = shadowImageViewHandle;
	deferredShadowsPassData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;
	deferredShadowsPassData.m_cascadeParamsBufferInfo = shadowCascadeParamsBufferInfo;

	DeferredShadowsPass2::addToGraph(graph, deferredShadowsPassData);

	// gtao
	GTAOModule2::Data gtaoModuleData;
	gtaoModuleData.m_passRecordContext = &passRecordContext;
	gtaoModuleData.m_depthImageViewHandle = depthImageViewHandle;
	gtaoModuleData.m_tangentSpaceImageViewHandle = tangentSpaceImageViewHandle;
	gtaoModuleData.m_velocityImageViewHandle = velocityImageViewHandle;

	if (g_ssaoEnabled)
	{
		m_gtaoModule->addToGraph(graph, gtaoModuleData);
	}

	// cull lights to tiles
	RasterTilingPass2::Data rasterTilingPassData;
	rasterTilingPassData.m_passRecordContext = &passRecordContext;
	rasterTilingPassData.m_lightData = &lightData;
	rasterTilingPassData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferViewHandle;

	if (!lightData.m_pointLightData.empty())
	{
		RasterTilingPass2::addToGraph(graph, rasterTilingPassData);
	}

	// light gbuffer
	LightingPass2::Data lightingPassData;
	lightingPassData.m_passRecordContext = &passRecordContext;
	lightingPassData.m_directionalLightDataBufferInfo = directionalLightDataBufferInfo;
	lightingPassData.m_pointLightDataBufferInfo = pointLightDataBufferInfo;
	lightingPassData.m_pointLightZBinsBufferInfo = pointLightZBinsBufferInfo;
	lightingPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };;
	lightingPassData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferViewHandle;
	lightingPassData.m_depthImageHandle = depthImageViewHandle;
	lightingPassData.m_uvImageHandle = uvImageViewHandle;
	lightingPassData.m_ddxyLengthImageHandle = ddxyLengthImageViewHandle;
	lightingPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageViewHandle;
	lightingPassData.m_tangentSpaceImageHandle = tangentSpaceImageViewHandle;
	lightingPassData.m_deferredShadowImageViewHandle = deferredShadowsImageViewHandle;
	lightingPassData.m_resultImageHandle = lightImageViewHandle;
	lightingPassData.m_albedoImageHandle = albedoImageViewHandle;
	lightingPassData.m_normalImageHandle = normalImageViewHandle;

	LightingPass2::addToGraph(graph, lightingPassData);


	// calculate luminance histograms
	LuminanceHistogramPass2::Data luminanceHistogramPassData;
	luminanceHistogramPassData.m_passRecordContext = &passRecordContext;
	luminanceHistogramPassData.m_lightImageHandle = lightImageViewHandle;
	luminanceHistogramPassData.m_luminanceHistogramBufferHandle = luminanceHistogramBufferViewHandle;

	LuminanceHistogramPass2::addToGraph(graph, luminanceHistogramPassData);


	// calculate avg luminance
	LuminanceHistogramAveragePass2::Data luminanceHistogramAvgPassData;
	luminanceHistogramAvgPassData.m_passRecordContext = &passRecordContext;
	luminanceHistogramAvgPassData.m_luminanceHistogramBufferHandle = luminanceHistogramBufferViewHandle;
	luminanceHistogramAvgPassData.m_avgLuminanceBufferHandle = avgLuminanceBufferViewHandle;

	LuminanceHistogramAveragePass2::addToGraph(graph, luminanceHistogramAvgPassData);


	// copy luminance histogram to readback buffer
	ReadBackCopyPass2::Data luminanceHistogramReadBackCopyPassData;
	luminanceHistogramReadBackCopyPassData.m_passRecordContext = &passRecordContext;
	luminanceHistogramReadBackCopyPassData.m_bufferCopy = { 0, 0, RendererConsts::LUMINANCE_HISTOGRAM_SIZE * sizeof(uint32_t) };
	luminanceHistogramReadBackCopyPassData.m_srcBuffer = luminanceHistogramBufferViewHandle;
	luminanceHistogramReadBackCopyPassData.m_dstBuffer = m_renderResources->m_luminanceHistogramReadBackBuffers[commonData.m_curResIdx];

	ReadBackCopyPass2::addToGraph(graph, luminanceHistogramReadBackCopyPassData);


	// get swapchain image
	rg::ImageViewHandle swapchainImageViewHandle = 0;
	{
		uint32_t swapChainImageIndex;
		m_swapChain->getCurrentImageIndex(swapChainImageIndex, m_semaphores[0], ++m_semaphoreValues[0]);

		auto imageHandle = graph.importImage(m_swapChain->getImage(m_swapChainImageIndex), "Swapchain Image", false, {});
		swapchainImageViewHandle = graph.createImageView({ "Swapchain Image", imageHandle, { 0, 1, 0, 1 } });
	}


	TAAPass2::Data taaPassData;
	taaPassData.m_passRecordContext = &passRecordContext;
	taaPassData.m_jitterOffsetX = commonData.m_jitteredProjectionMatrix[2][0];
	taaPassData.m_jitterOffsetY = commonData.m_jitteredProjectionMatrix[2][1];
	taaPassData.m_depthImageHandle = depthImageViewHandle;
	taaPassData.m_velocityImageHandle = velocityImageViewHandle;
	taaPassData.m_taaHistoryImageHandle = taaHistoryImageViewHandle;
	taaPassData.m_lightImageHandle = lightImageViewHandle;
	taaPassData.m_taaResolveImageHandle = taaResolveImageViewHandle;

	if (g_TAAEnabled)
	{
		TAAPass2::addToGraph(graph, taaPassData);

		lightImageViewHandle = taaResolveImageViewHandle;
	}


	rg::ImageViewHandle currentOutput = lightImageViewHandle;


	// bloom
	rg::ImageViewHandle bloomImageViewHandle = 0;
	BloomModule2::OutputData bloomModuleOutData;
	BloomModule2::InputData bloomModuleInData;
	bloomModuleInData.m_passRecordContext = &passRecordContext;
	bloomModuleInData.m_colorImageViewHandle = currentOutput;

	if (g_bloomEnabled)
	{
		BloomModule2::addToGraph(graph, bloomModuleInData, bloomModuleOutData);
		bloomImageViewHandle = bloomModuleOutData.m_bloomImageViewHandle;
	}


	rg::ImageViewHandle tonemapTargetTextureHandle = g_FXAAEnabled || g_CASEnabled ? finalImageViewHandle : swapchainImageViewHandle;

	// tonemap
	TonemapPass2::Data tonemapPassData;
	tonemapPassData.m_passRecordContext = &passRecordContext;
	tonemapPassData.m_bloomEnabled = g_bloomEnabled;
	tonemapPassData.m_bloomStrength = g_bloomStrength;
	tonemapPassData.m_applyLinearToGamma = g_FXAAEnabled || !g_CASEnabled;
	tonemapPassData.m_srcImageHandle = currentOutput;
	tonemapPassData.m_dstImageHandle = tonemapTargetTextureHandle;
	tonemapPassData.m_bloomImageViewHandle = bloomImageViewHandle;
	tonemapPassData.m_avgLuminanceBufferHandle = avgLuminanceBufferViewHandle;

	TonemapPass2::addToGraph(graph, tonemapPassData);

	currentOutput = tonemapTargetTextureHandle;


	rg::ImageViewHandle fxaaTargetTextureHandle = g_CASEnabled ? finalImageViewHandle2 : swapchainImageViewHandle;

	// FXAA
	FXAAPass2::Data fxaaPassData;
	fxaaPassData.m_passRecordContext = &passRecordContext;
	fxaaPassData.m_inputImageHandle = currentOutput;
	fxaaPassData.m_resultImageHandle = fxaaTargetTextureHandle;

	if (g_FXAAEnabled)
	{
		FXAAPass2::addToGraph(graph, fxaaPassData);
		currentOutput = fxaaTargetTextureHandle;
	}


	// Sharpen (CAS)
	SharpenFfxCasPass2::Data sharpenFfxCasPassData;
	sharpenFfxCasPassData.m_passRecordContext = &passRecordContext;
	sharpenFfxCasPassData.m_gammaSpaceInput = tonemapPassData.m_applyLinearToGamma;
	sharpenFfxCasPassData.m_sharpness = g_CASSharpness;
	sharpenFfxCasPassData.m_srcImageHandle = currentOutput;
	sharpenFfxCasPassData.m_dstImageHandle = swapchainImageViewHandle;

	if (g_CASEnabled)
	{
		SharpenFfxCasPass2::addToGraph(graph, sharpenFfxCasPassData);
	}


	// ImGui
	ImGuiPass2::Data imGuiPassData;
	imGuiPassData.m_passRecordContext = &passRecordContext;
	imGuiPassData.m_imGuiDrawData = ImGui::GetDrawData();
	imGuiPassData.m_resultImageViewHandle = swapchainImageViewHandle;

	ImGuiPass2::addToGraph(graph, imGuiPassData);

	graph.execute(rg::ResourceViewHandle(swapchainImageViewHandle), { {ResourceState::PRESENT_IMAGE, PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT}, m_graphicsDevice->getGraphicsQueue() });
	
	m_swapChain->present(m_semaphores[0], m_semaphoreValues[0]);
}

VEngine::TextureHandle VEngine::Renderer::loadTexture(const char *filepath)
{
	return m_textureLoader->load(filepath);
}

void VEngine::Renderer::freeTexture(TextureHandle id)
{
	m_textureLoader->free(id);
}

void VEngine::Renderer::createMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	m_materialManager->createMaterials(count, materials, handles);
}

void VEngine::Renderer::updateMaterials(uint32_t count, const Material *materials, MaterialHandle *handles)
{
	m_materialManager->updateMaterials(count, materials, handles);
}

void VEngine::Renderer::destroyMaterials(uint32_t count, MaterialHandle *handles)
{
	m_materialManager->destroyMaterials(count, handles);
}

void VEngine::Renderer::createSubMeshes(uint32_t count, SubMesh *subMeshes, SubMeshHandle *handles)
{
	m_meshManager->createSubMeshes(count, subMeshes, handles);
}

void VEngine::Renderer::destroySubMeshes(uint32_t count, SubMeshHandle *handles)
{
	m_meshManager->destroySubMeshes(count, handles);
}

void VEngine::Renderer::updateTextureData()
{
	m_renderResources->updateTextureArray(RendererConsts::TEXTURE_ARRAY_SIZE, m_textureLoader->getViews());
}

const uint32_t *VEngine::Renderer::getLuminanceHistogram() const
{
	return m_luminanceHistogram;
}

void VEngine::Renderer::getTimingInfo(size_t *count, const PassTimingInfo **data) const
{
	*count = m_passTimingCount;
	*data = m_passTimingData;
}

void VEngine::Renderer::getOcclusionCullingStats(uint32_t &draws, uint32_t &totalDraws) const
{
	draws = m_opaqueDraws;
	totalDraws = m_totalOpaqueDraws;
}

void VEngine::Renderer::setBVH(uint32_t nodeCount, const BVHNode *nodes, uint32_t triangleCount, const Triangle *triangles)
{
	m_renderResources->setBVH(nodeCount, nodes, triangleCount, triangles);
}

void VEngine::Renderer::resize(uint32_t width, uint32_t height)
{
	m_graphicsDevice->waitIdle();
	m_width = width;
	m_height = height;
	if (m_width > 0 && m_height > 0)
	{
		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			m_frameGraphs[i]->reset();
		}
		m_swapChain->resize(width, height);
		m_renderResources->resize(width, height);
		m_gtaoModule->resize(width, height);
		m_ssrModule->resize(width, height);
	}
}