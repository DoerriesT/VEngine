#include "Renderer.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "RenderResources.h"
#include "Utility/Utility.h"
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"
#include "TextureLoader.h"
#include "GlobalVar.h"
#include "Pass/IntegrateBrdfPass.h"
#include "Pass/ShadowPass.h"
#include "Pass/ShadowAtlasPass.h"
#include "Pass/RasterTilingPass.h"
#include "Pass/LuminanceHistogramPass.h"
#include "Pass/ExposurePass.h"
#include "Pass/TonemapPass.h"
#include "Pass/VelocityInitializationPass.h"
#include "Pass/FXAAPass.h"
#include "Pass/DeferredShadowsPass.h"
#include "Pass/ImGuiPass.h"
#include "Pass/ReadBackCopyPass.h"
#include "Pass/HiZPyramidPass.h"
#include "Pass/BuildIndexBufferPass.h"
#include "Pass/SharpenFfxCasPass.h"
#include "Pass/TemporalAAPass.h"
#include "Pass/DepthPrepassPass.h"
#include "Pass/ForwardLightingPass.h"
#include "Pass/VolumetricFogApplyPass.h"
#include "Pass/GaussianDownsamplePass.h"
#include "Module/GTAOModule.h"
#include "Module/SSRModule.h"
#include "Module/BloomModule.h"
#include "Module/VolumetricFogModule.h"
#include "PipelineCache.h"
#include "DescriptorSetCache.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include "RenderGraph.h"
#include "PassRecordContext.h"
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
	:m_graphicsDevice(GraphicsDevice::create(windowHandle, true, GraphicsBackendType::VULKAN)),
	m_framesSinceLastResize()
{
	m_graphicsDevice->createSwapChain(m_graphicsDevice->getGraphicsQueue(), width, height, &m_swapChain);

	m_graphicsDevice->createSemaphore(0, &m_semaphores[0]);
	m_graphicsDevice->createSemaphore(0, &m_semaphores[1]);
	m_graphicsDevice->createSemaphore(0, &m_semaphores[2]);

	m_graphicsDevice->setDebugObjectName(ObjectType::SEMAPHORE, m_semaphores[0], "Graphics Queue Semaphore");
	m_graphicsDevice->setDebugObjectName(ObjectType::SEMAPHORE, m_semaphores[1], "Compute Queue Semaphore");
	m_graphicsDevice->setDebugObjectName(ObjectType::SEMAPHORE, m_semaphores[2], "Transfer Queue Semaphore");

	m_width = m_swapChain->getExtent().m_width;
	m_height = m_swapChain->getExtent().m_height;

	m_renderResources = std::make_unique<RenderResources>(m_graphicsDevice);
	m_renderResources->init(m_width, m_height);

	m_pipelineCache = std::make_unique<PipelineCache>(m_graphicsDevice);
	m_descriptorSetCache = std::make_unique<DescriptorSetCache>(m_graphicsDevice);
	m_textureLoader = std::make_unique<TextureLoader>(m_graphicsDevice, m_renderResources->m_stagingBuffer);
	m_materialManager = std::make_unique<MaterialManager>(m_graphicsDevice, m_renderResources->m_stagingBuffer, m_renderResources->m_materialBuffer);
	m_meshManager = std::make_unique<MeshManager>(m_graphicsDevice, m_renderResources->m_stagingBuffer, m_renderResources->m_vertexBuffer, m_renderResources->m_indexBuffer, m_renderResources->m_subMeshDataInfoBuffer, m_renderResources->m_subMeshBoundingBoxBuffer);


	m_fontAtlasTextureIndex = m_textureLoader->load("Resources/Textures/fontConsolas.dds");
	m_blueNoiseTextureIndex = m_textureLoader->load("Resources/Textures/blue_noise_LDR_RGBA_0.dds");

	updateTextureData();

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_frameGraphs[i] = std::make_unique<rg::RenderGraph>(m_graphicsDevice, m_semaphores, m_semaphoreValues);
	}

	m_gtaoModule = std::make_unique<GTAOModule>(m_graphicsDevice, m_width, m_height);
	m_ssrModule = std::make_unique<SSRModule>(m_graphicsDevice, m_width, m_height);
	m_volumetricFogModule = std::make_unique<VolumetricFogModule>(m_graphicsDevice, m_width, m_height);
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
	m_volumetricFogModule.reset();

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

	// get swapchain image
	rg::ResourceStateData swapChainState{ {}, m_graphicsDevice->getGraphicsQueue() };
	rg::ImageViewHandle swapchainImageViewHandle = 0;
	{
		auto imageHandle = graph.importImage(m_swapChain->getImage(m_swapChain->getCurrentImageIndex()), "Swapchain Image", false, {}, &swapChainState);
		swapchainImageViewHandle = graph.createImageView({ "Swapchain Image", imageHandle, { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle shadowAtlasImageViewHandle = 0;
	{
		rg::ImageHandle imageHandle = graph.importImage(m_renderResources->m_shadowAtlasImage, "Shadow Atlas", false, {}, &m_renderResources->m_shadowAtlasImageState);
		shadowAtlasImageViewHandle = graph.createImageView({ "Shadow Atlas", imageHandle, { 0, 1, 0, 1 } });
	}

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

	rg::ImageHandle lightImageHandle = 0;
	rg::ImageViewHandle lightImageViewHandle = 0;
	{
		lightImageHandle = graph.importImage(m_renderResources->m_lightImages[commonData.m_curResIdx], "Light Image", false, {}, m_renderResources->m_lightImageState[commonData.m_curResIdx]);
		lightImageViewHandle = graph.createImageView({ "Light Image", lightImageHandle, {  0, 1, 0, 1 } });
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

	rg::BufferViewHandle exposureDataBufferViewHandle = 0;
	{
		rg::BufferHandle exposureDataBufferHandle = graph.importBuffer(m_renderResources->m_exposureDataBuffer, "Exposure Data Buffer", false, {}, &m_renderResources->m_exposureDataBufferState);
		exposureDataBufferViewHandle = graph.createBufferView({ "Exposure Data Buffer", exposureDataBufferHandle, 0, m_renderResources->m_exposureDataBuffer->getDescription().m_size });
	}

	rg::ImageViewHandle brdfLUTImageViewHandle = 0;
	{
		rg::ImageHandle imageHandle = graph.importImage(m_renderResources->m_brdfLUT, "BRDF LUT Image", false, {}, &m_renderResources->m_brdfLutImageState);
		brdfLUTImageViewHandle = graph.createImageView({ "BRDF LUT Image", imageHandle, { 0, 1, 0, 1 } });
	}

	// create graph managed resources

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
		desc.m_format = Format::R16G16_SFLOAT;

		normalImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle specularRoughnessImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "Specular/Roughness Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R8G8B8A8_UNORM;

		specularRoughnessImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
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
		desc.m_layers = glm::max(static_cast<uint32_t>(lightData.m_directionalLightsShadowed.size()), 1u);
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R8_UNORM;

		deferredShadowsImageHandle = graph.createImage(desc);
		deferredShadowsImageViewHandle = graph.createImageView({ desc.m_name, deferredShadowsImageHandle, { 0, 1, 0, desc.m_layers }, ImageViewType::_2D_ARRAY });
	}

	rg::ImageViewHandle finalImageViewHandle = ResourceDefinitions::createFinalImageViewHandle(graph, m_width, m_height);
	rg::ImageViewHandle finalImageViewHandle2 = ResourceDefinitions::createFinalImageViewHandle(graph, m_width, m_height);
	//rg::ImageViewHandle uvImageViewHandle = ResourceDefinitions::createUVImageViewHandle(graph, m_width, m_height);
	//rg::ImageViewHandle ddxyLengthImageViewHandle = ResourceDefinitions::createDerivativesLengthImageViewHandle(graph, m_width, m_height);
	//rg::ImageViewHandle ddxyRotMaterialIdImageViewHandle = ResourceDefinitions::createDerivativesRotMaterialIdImageViewHandle(graph, m_width, m_height);
	//rg::ImageViewHandle tangentSpaceImageViewHandle = ResourceDefinitions::createTangentSpaceImageViewHandle(graph, m_width, m_height);
	rg::ImageViewHandle velocityImageViewHandle = ResourceDefinitions::createVelocityImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle lightImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyAccumImageViewHandle = VKResourceDefinitions::createTransparencyAccumImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyTransmittanceImageViewHandle = VKResourceDefinitions::createTransparencyTransmittanceImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyDeltaImageViewHandle = VKResourceDefinitions::createTransparencyDeltaImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyResultImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle deferredShadowsImageViewHandle = VKResourceDefinitions::createDeferredShadowsImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle reprojectedDepthUintImageViewHandle = VKResourceDefinitions::createReprojectedDepthUintImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle reprojectedDepthImageViewHandle = VKResourceDefinitions::createReprojectedDepthImageViewHandle(graph, m_width, m_height);
	rg::BufferViewHandle punctualLightBitMaskBufferViewHandle = ResourceDefinitions::createTiledLightingBitMaskBufferViewHandle(graph, m_width, m_height, static_cast<uint32_t>(lightData.m_punctualLights.size()));
	rg::BufferViewHandle punctualLightShadowedBitMaskBufferViewHandle = ResourceDefinitions::createTiledLightingBitMaskBufferViewHandle(graph, m_width, m_height, static_cast<uint32_t>(lightData.m_punctualLightsShadowed.size()));
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
	DescriptorBufferInfo directionalLightsBufferInfo{ nullptr, 0, std::max(lightData.m_directionalLights.size() * sizeof(DirectionalLight), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(directionalLightsBufferInfo.m_range, directionalLightsBufferInfo.m_offset, directionalLightsBufferInfo.m_buffer, bufferPtr);
		if (!lightData.m_directionalLights.empty())
		{
			memcpy(bufferPtr, lightData.m_directionalLights.data(), lightData.m_directionalLights.size() * sizeof(DirectionalLight));
		}
	}

	// shadowed directional light data write
	DescriptorBufferInfo directionalLightsShadowedBufferInfo{ nullptr, 0, std::max(lightData.m_directionalLightsShadowed.size() * sizeof(DirectionalLight), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(directionalLightsShadowedBufferInfo.m_range, directionalLightsShadowedBufferInfo.m_offset, directionalLightsShadowedBufferInfo.m_buffer, bufferPtr);
		if (!lightData.m_directionalLightsShadowed.empty())
		{
			memcpy(bufferPtr, lightData.m_directionalLightsShadowed.data(), lightData.m_directionalLightsShadowed.size() * sizeof(DirectionalLight));
		}
	}

	// punctual light data write
	DescriptorBufferInfo punctualLightDataBufferInfo{ nullptr, 0, std::max(lightData.m_punctualLights.size() * sizeof(PunctualLight), size_t(1)) };
	DescriptorBufferInfo punctualLightZBinsBufferInfo{ nullptr, 0, std::max(lightData.m_punctualLightDepthBins.size() * sizeof(uint32_t), size_t(1)) };
	{
		uint8_t *dataBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(punctualLightDataBufferInfo.m_range, punctualLightDataBufferInfo.m_offset, punctualLightDataBufferInfo.m_buffer, dataBufferPtr);
		uint8_t *zBinsBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(punctualLightZBinsBufferInfo.m_range, punctualLightZBinsBufferInfo.m_offset, punctualLightZBinsBufferInfo.m_buffer, zBinsBufferPtr);
		if (!lightData.m_punctualLights.empty())
		{
			for (size_t i = 0; i < lightData.m_punctualLightOrder.size(); ++i)
			{
				((PunctualLight *)dataBufferPtr)[i] = lightData.m_punctualLights[lightData.m_punctualLightOrder[i]];
			}
			//memcpy(dataBufferPtr, lightData.m_pointLightData.data(), lightData.m_pointLightData.size() * sizeof(PointLightData));
			memcpy(zBinsBufferPtr, lightData.m_punctualLightDepthBins.data(), lightData.m_punctualLightDepthBins.size() * sizeof(uint32_t));
		}
	}

	// shadowed punctual light data write
	DescriptorBufferInfo punctualLightShadowedDataBufferInfo{ nullptr, 0, std::max(lightData.m_punctualLightsShadowed.size() * sizeof(PunctualLightShadowed), size_t(1)) };
	DescriptorBufferInfo punctualLightShadowedZBinsBufferInfo{ nullptr, 0, std::max(lightData.m_punctualLightShadowedDepthBins.size() * sizeof(uint32_t), size_t(1)) };
	{
		uint8_t *dataBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(punctualLightShadowedDataBufferInfo.m_range, punctualLightShadowedDataBufferInfo.m_offset, punctualLightShadowedDataBufferInfo.m_buffer, dataBufferPtr);
		uint8_t *zBinsBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(punctualLightShadowedZBinsBufferInfo.m_range, punctualLightShadowedZBinsBufferInfo.m_offset, punctualLightShadowedZBinsBufferInfo.m_buffer, zBinsBufferPtr);
		if (!lightData.m_punctualLightsShadowed.empty())
		{
			for (size_t i = 0; i < lightData.m_punctualLightShadowedOrder.size(); ++i)
			{
				((PunctualLightShadowed *)dataBufferPtr)[i] = lightData.m_punctualLightsShadowed[lightData.m_punctualLightShadowedOrder[i]];
			}
			//memcpy(dataBufferPtr, lightData.m_pointLightData.data(), lightData.m_pointLightData.size() * sizeof(PointLightData));
			memcpy(zBinsBufferPtr, lightData.m_punctualLightShadowedDepthBins.data(), lightData.m_punctualLightShadowedDepthBins.size() * sizeof(uint32_t));
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

	// shadow cascade params write
	DescriptorBufferInfo shadowCascadeParamsBufferInfo{ nullptr, 0, std::max(renderData.m_shadowCascadeViewRenderListCount * sizeof(glm::vec4), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(shadowCascadeParamsBufferInfo.m_range, shadowCascadeParamsBufferInfo.m_offset, shadowCascadeParamsBufferInfo.m_buffer, bufferPtr);
		if (renderData.m_shadowCascadeViewRenderListCount)
		{
			memcpy(bufferPtr, renderData.m_shadowCascadeParams, renderData.m_shadowCascadeViewRenderListCount * sizeof(glm::vec4));
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

	PassRecordContext passRecordContext{};
	passRecordContext.m_renderResources = m_renderResources.get();
	passRecordContext.m_pipelineCache = m_pipelineCache.get();
	passRecordContext.m_descriptorSetCache = m_descriptorSetCache.get();
	passRecordContext.m_commonRenderData = &commonData;

	if (commonData.m_frame == 0)
	{
		IntegrateBrdfPass::Data integrateBrdfPassData;
		integrateBrdfPassData.m_passRecordContext = &passRecordContext;
		integrateBrdfPassData.m_resultImageViewHandle = brdfLUTImageViewHandle;

		IntegrateBrdfPass::addToGraph(graph, integrateBrdfPassData);
	}

	// Hi-Z furthest depth pyramid
	HiZPyramidPass::OutData hiZMinPyramidPassOutData;
	HiZPyramidPass::Data hiZMinPyramidPassData;
	hiZMinPyramidPassData.m_passRecordContext = &passRecordContext;
	hiZMinPyramidPassData.m_inputImageViewHandle = prevDepthImageViewHandle;
	hiZMinPyramidPassData.m_maxReduction = false;
	hiZMinPyramidPassData.m_copyFirstLevel = false;
	hiZMinPyramidPassData.m_forceExecution = true;

	HiZPyramidPass::addToGraph(graph, hiZMinPyramidPassData, hiZMinPyramidPassOutData);

	rg::ImageViewHandle depthPyramidImageViewHandle = hiZMinPyramidPassOutData.m_resultImageViewHandle;


	// depth prepass
	DepthPrepassPass::Data depthPrePassData;
	depthPrePassData.m_passRecordContext = &passRecordContext;
	depthPrePassData.m_opaqueInstanceDataCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount;
	depthPrePassData.m_opaqueInstanceDataOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueOffset;
	depthPrePassData.m_maskedInstanceDataCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedCount;
	depthPrePassData.m_maskedInstanceDataOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedOffset;
	depthPrePassData.m_instanceData = sortedInstanceData.data();
	depthPrePassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
	depthPrePassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
	depthPrePassData.m_transformDataBufferInfo = transformDataBufferInfo;
	depthPrePassData.m_depthImageHandle = depthImageViewHandle;

	DepthPrepassPass::addToGraph(graph, depthPrePassData);


	// initialize velocity of static objects
	VelocityInitializationPass::Data velocityInitializationPassData;
	velocityInitializationPassData.m_passRecordContext = &passRecordContext;
	velocityInitializationPassData.m_depthImageHandle = depthImageViewHandle;
	velocityInitializationPassData.m_velocityImageHandle = velocityImageViewHandle;

	VelocityInitializationPass::addToGraph(graph, velocityInitializationPassData);



	// cull lights to tiles
	RasterTilingPass::Data rasterTilingPassData;
	rasterTilingPassData.m_passRecordContext = &passRecordContext;
	rasterTilingPassData.m_lightData = &lightData;
	rasterTilingPassData.m_punctualLightsBitMaskBufferHandle = punctualLightBitMaskBufferViewHandle;
	rasterTilingPassData.m_punctualLightsShadowedBitMaskBufferHandle = punctualLightShadowedBitMaskBufferViewHandle;

	if (!lightData.m_punctualLights.empty() || !lightData.m_punctualLightsShadowed.empty())
	{
		RasterTilingPass::addToGraph(graph, rasterTilingPassData);
	}


	// shadow atlas pass
	ShadowAtlasPass::Data shadowAtlasPassData;
	shadowAtlasPassData.m_passRecordContext = &passRecordContext;
	shadowAtlasPassData.m_shadowMapSize = 8192;
	shadowAtlasPassData.m_drawInfoCount = lightData.m_shadowAtlasDrawInfos.size();
	shadowAtlasPassData.m_shadowAtlasDrawInfo = lightData.m_shadowAtlasDrawInfos.data();
	shadowAtlasPassData.m_renderLists = renderData.m_renderLists;
	shadowAtlasPassData.m_shadowMatrices = renderData.m_shadowMatrices;
	shadowAtlasPassData.m_instanceData = sortedInstanceData.data();
	shadowAtlasPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
	shadowAtlasPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
	shadowAtlasPassData.m_transformDataBufferInfo = transformDataBufferInfo;
	shadowAtlasPassData.m_shadowAtlasImageViewHandle = shadowAtlasImageViewHandle;

	if (!lightData.m_shadowAtlasDrawInfos.empty())
	{
		ShadowAtlasPass::addToGraph(graph, shadowAtlasPassData);
	}


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
			ShadowPass::Data shadowPassData;
			shadowPassData.m_passRecordContext = &passRecordContext;
			shadowPassData.m_shadowMapSize = 2048;
			shadowPassData.m_shadowMatrix = renderData.m_shadowMatrices[i];
			shadowPassData.m_opaqueInstanceDataCount = drawList.m_opaqueCount;
			shadowPassData.m_opaqueInstanceDataOffset = drawList.m_opaqueOffset;
			shadowPassData.m_maskedInstanceDataCount = drawList.m_maskedCount;
			shadowPassData.m_maskedInstanceDataOffset = drawList.m_maskedOffset;
			shadowPassData.m_instanceData = sortedInstanceData.data();
			shadowPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
			shadowPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
			shadowPassData.m_transformDataBufferInfo = transformDataBufferInfo;
			shadowPassData.m_shadowImageHandle = shadowLayer;

			ShadowPass::addToGraph(graph, shadowPassData);
		}
	}


	// deferred shadows
	DeferredShadowsPass::Data deferredShadowsPassData;
	deferredShadowsPassData.m_passRecordContext = &passRecordContext;
	deferredShadowsPassData.m_lightDataCount = static_cast<uint32_t>(lightData.m_directionalLightsShadowed.size());
	deferredShadowsPassData.m_lightData = lightData.m_directionalLightsShadowed.data();
	deferredShadowsPassData.m_resultImageHandle = deferredShadowsImageHandle;
	deferredShadowsPassData.m_depthImageViewHandle = depthImageViewHandle;
	//deferredShadowsPassData.m_tangentSpaceImageViewHandle = tangentSpaceImageViewHandle;
	deferredShadowsPassData.m_shadowImageViewHandle = shadowImageViewHandle;
	deferredShadowsPassData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;
	deferredShadowsPassData.m_cascadeParamsBufferInfo = shadowCascadeParamsBufferInfo;

	DeferredShadowsPass::addToGraph(graph, deferredShadowsPassData);


	// gtao
	GTAOModule::Data gtaoModuleData;
	gtaoModuleData.m_passRecordContext = &passRecordContext;
	gtaoModuleData.m_ignoreHistory = m_framesSinceLastResize < RendererConsts::FRAMES_IN_FLIGHT;
	gtaoModuleData.m_depthImageViewHandle = depthImageViewHandle;
	//gtaoModuleData.m_tangentSpaceImageViewHandle = tangentSpaceImageViewHandle;
	gtaoModuleData.m_velocityImageViewHandle = velocityImageViewHandle;

	if (g_ssaoEnabled)
	{
		m_gtaoModule->addToGraph(graph, gtaoModuleData);
	}


	// volumetric fog
	VolumetricFogModule::Data volumetricFogModuleData;
	volumetricFogModuleData.m_passRecordContext = &passRecordContext;
	volumetricFogModuleData.m_ignoreHistory = m_framesSinceLastResize < RendererConsts::FRAMES_IN_FLIGHT;
	volumetricFogModuleData.m_commonData = &commonData;
	volumetricFogModuleData.m_shadowImageViewHandle = shadowImageViewHandle;
	volumetricFogModuleData.m_shadowAtlasImageViewHandle = shadowAtlasImageViewHandle;
	volumetricFogModuleData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	volumetricFogModuleData.m_punctualLightsBitMaskBufferHandle = punctualLightBitMaskBufferViewHandle;
	volumetricFogModuleData.m_punctualLightsShadowedBitMaskBufferHandle = punctualLightShadowedBitMaskBufferViewHandle;
	volumetricFogModuleData.m_directionalLightsBufferInfo = directionalLightsBufferInfo;
	volumetricFogModuleData.m_directionalLightsShadowedBufferInfo = directionalLightsShadowedBufferInfo;
	volumetricFogModuleData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;
	volumetricFogModuleData.m_punctualLightsBufferInfo = punctualLightDataBufferInfo;
	volumetricFogModuleData.m_punctualLightsZBinsBufferInfo = punctualLightZBinsBufferInfo;
	volumetricFogModuleData.m_punctualLightsShadowedBufferInfo = punctualLightShadowedDataBufferInfo;
	volumetricFogModuleData.m_punctualLightsShadowedZBinsBufferInfo = punctualLightShadowedZBinsBufferInfo;

	m_volumetricFogModule->addToGraph(graph, volumetricFogModuleData);


	// forward lighting
	ForwardLightingPass::Data forwardPassData;
	forwardPassData.m_passRecordContext = &passRecordContext;
	forwardPassData.m_ssao = g_ssaoEnabled;
	forwardPassData.m_instanceDataCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount
		+ renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedCount; // masked entities come right after opaque entities in the list, so we can just add the counts
	forwardPassData.m_instanceDataOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueOffset;
	forwardPassData.m_instanceData = sortedInstanceData.data();
	forwardPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
	forwardPassData.m_directionalLightsBufferInfo = directionalLightsBufferInfo;
	forwardPassData.m_directionalLightsShadowedBufferInfo = directionalLightsShadowedBufferInfo;
	forwardPassData.m_punctualLightsBufferInfo = punctualLightDataBufferInfo;
	forwardPassData.m_punctualLightsZBinsBufferInfo = punctualLightZBinsBufferInfo;
	forwardPassData.m_punctualLightsShadowedBufferInfo = punctualLightShadowedDataBufferInfo;
	forwardPassData.m_punctualLightsShadowedZBinsBufferInfo = punctualLightShadowedZBinsBufferInfo;
	forwardPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
	forwardPassData.m_transformDataBufferInfo = transformDataBufferInfo;
	forwardPassData.m_punctualLightsBitMaskBufferHandle = punctualLightBitMaskBufferViewHandle;
	forwardPassData.m_punctualLightsShadowedBitMaskBufferHandle = punctualLightShadowedBitMaskBufferViewHandle;
	forwardPassData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	forwardPassData.m_deferredShadowImageViewHandle = deferredShadowsImageViewHandle;
	forwardPassData.m_depthImageViewHandle = depthImageViewHandle;
	forwardPassData.m_resultImageViewHandle = lightImageViewHandle;
	forwardPassData.m_normalImageViewHandle = normalImageViewHandle;
	forwardPassData.m_specularRoughnessImageViewHandle = specularRoughnessImageViewHandle;
	//forwardPassData.m_volumetricFogImageViewHandle = m_volumetricFogModule->getVolumetricScatteringImageViewHandle();
	forwardPassData.m_ssaoImageViewHandle = m_gtaoModule->getAOResultImageViewHandle(); // TODO: what to pass in when ssao is disabled?
	forwardPassData.m_shadowAtlasImageViewHandle = shadowAtlasImageViewHandle;

	ForwardLightingPass::addToGraph(graph, forwardPassData);


	// Hi-Z nearest depth pyramid
	HiZPyramidPass::OutData hiZMaxPyramidPassOutData;
	HiZPyramidPass::Data hiZMaxPyramidPassData;
	hiZMaxPyramidPassData.m_passRecordContext = &passRecordContext;
	hiZMaxPyramidPassData.m_inputImageViewHandle = depthImageViewHandle;
	hiZMaxPyramidPassData.m_maxReduction = true;
	hiZMaxPyramidPassData.m_copyFirstLevel = true;
	hiZMaxPyramidPassData.m_forceExecution = false;

	HiZPyramidPass::addToGraph(graph, hiZMaxPyramidPassData, hiZMaxPyramidPassOutData);


	// screen space reflections
	SSRModule::Data ssrModuleData;
	ssrModuleData.m_passRecordContext = &passRecordContext;
	ssrModuleData.m_ignoreHistory = m_framesSinceLastResize < RendererConsts::FRAMES_IN_FLIGHT;
	ssrModuleData.m_noiseTextureHandle = m_blueNoiseTextureIndex;
	ssrModuleData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	ssrModuleData.m_hiZPyramidImageViewHandle = hiZMaxPyramidPassOutData.m_resultImageViewHandle;
	ssrModuleData.m_normalImageViewHandle = normalImageViewHandle;
	ssrModuleData.m_depthImageViewHandle = depthImageViewHandle;
	ssrModuleData.m_specularRoughnessImageViewHandle = specularRoughnessImageViewHandle; 
	ssrModuleData.m_prevColorImageViewHandle = prevLightImageViewHandle;
	ssrModuleData.m_velocityImageViewHandle = velocityImageViewHandle;
	
	m_ssrModule->addToGraph(graph, ssrModuleData);


	// apply volumetric fog and indirect specular light to scene
	VolumetricFogApplyPass::Data volumetricFogApplyPassData;
	volumetricFogApplyPassData.m_passRecordContext = &passRecordContext;
	volumetricFogApplyPassData.m_noiseTextureHandle = m_blueNoiseTextureIndex;
	volumetricFogApplyPassData.m_depthImageViewHandle = depthImageViewHandle;
	volumetricFogApplyPassData.m_volumetricFogImageViewHandle = m_volumetricFogModule->getVolumetricScatteringImageViewHandle();
	volumetricFogApplyPassData.m_indirectSpecularLightImageViewHandle = m_ssrModule->getSSRResultImageViewHandle();
	volumetricFogApplyPassData.m_brdfLutImageViewHandle = brdfLUTImageViewHandle;
	volumetricFogApplyPassData.m_specularRoughnessImageViewHandle = specularRoughnessImageViewHandle;
	volumetricFogApplyPassData.m_normalImageViewHandle = normalImageViewHandle;
	volumetricFogApplyPassData.m_resultImageHandle = lightImageViewHandle;

	VolumetricFogApplyPass::addToGraph(graph, volumetricFogApplyPassData);


	GaussianDownsamplePass::Data gaussianDownsamplePassData;
	gaussianDownsamplePassData.m_passRecordContext = &passRecordContext;
	gaussianDownsamplePassData.m_width = m_width;
	gaussianDownsamplePassData.m_height = m_height;
	gaussianDownsamplePassData.m_levels = m_renderResources->m_lightImages[0]->getDescription().m_levels;
	gaussianDownsamplePassData.m_format = m_renderResources->m_lightImages[0]->getDescription().m_format;
	gaussianDownsamplePassData.m_resultImageHandle = lightImageHandle;

	GaussianDownsamplePass::addToGraph(graph, gaussianDownsamplePassData);


	// calculate luminance histograms
	LuminanceHistogramPass::Data luminanceHistogramPassData;
	luminanceHistogramPassData.m_passRecordContext = &passRecordContext;
	luminanceHistogramPassData.m_lightImageHandle = lightImageViewHandle;
	luminanceHistogramPassData.m_luminanceHistogramBufferHandle = luminanceHistogramBufferViewHandle;
	luminanceHistogramPassData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;

	LuminanceHistogramPass::addToGraph(graph, luminanceHistogramPassData);


	// calculate avg luminance and exposure
	ExposurePass::Data exposurePassData;
	exposurePassData.m_passRecordContext = &passRecordContext;
	exposurePassData.m_luminanceHistogramBufferHandle = luminanceHistogramBufferViewHandle;
	exposurePassData.m_avgLuminanceBufferHandle = avgLuminanceBufferViewHandle;
	exposurePassData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;

	ExposurePass::addToGraph(graph, exposurePassData);


	// copy luminance histogram to readback buffer
	ReadBackCopyPass::Data luminanceHistogramReadBackCopyPassData;
	luminanceHistogramReadBackCopyPassData.m_passRecordContext = &passRecordContext;
	luminanceHistogramReadBackCopyPassData.m_bufferCopy = { 0, 0, RendererConsts::LUMINANCE_HISTOGRAM_SIZE * sizeof(uint32_t) };
	luminanceHistogramReadBackCopyPassData.m_srcBuffer = luminanceHistogramBufferViewHandle;
	luminanceHistogramReadBackCopyPassData.m_dstBuffer = m_renderResources->m_luminanceHistogramReadBackBuffers[commonData.m_curResIdx];

	ReadBackCopyPass::addToGraph(graph, luminanceHistogramReadBackCopyPassData);


	TemporalAAPass::Data temporalAAPassData;
	temporalAAPassData.m_passRecordContext = &passRecordContext;
	temporalAAPassData.m_ignoreHistory = m_framesSinceLastResize < RendererConsts::FRAMES_IN_FLIGHT;
	temporalAAPassData.m_jitterOffsetX = commonData.m_jitteredProjectionMatrix[2][0];
	temporalAAPassData.m_jitterOffsetY = commonData.m_jitteredProjectionMatrix[2][1];
	temporalAAPassData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	temporalAAPassData.m_depthImageHandle = depthImageViewHandle;
	temporalAAPassData.m_velocityImageHandle = velocityImageViewHandle;
	temporalAAPassData.m_taaHistoryImageHandle = taaHistoryImageViewHandle;
	temporalAAPassData.m_lightImageHandle = lightImageViewHandle;
	temporalAAPassData.m_taaResolveImageHandle = taaResolveImageViewHandle;

	if (g_TAAEnabled)
	{
		TemporalAAPass::addToGraph(graph, temporalAAPassData);

		lightImageViewHandle = taaResolveImageViewHandle;
	}


	rg::ImageViewHandle currentOutput = lightImageViewHandle;


	// bloom
	rg::ImageViewHandle bloomImageViewHandle = 0;
	BloomModule::OutputData bloomModuleOutData;
	BloomModule::InputData bloomModuleInData;
	bloomModuleInData.m_passRecordContext = &passRecordContext;
	bloomModuleInData.m_colorImageViewHandle = currentOutput;

	if (g_bloomEnabled)
	{
		BloomModule::addToGraph(graph, bloomModuleInData, bloomModuleOutData);
		bloomImageViewHandle = bloomModuleOutData.m_bloomImageViewHandle;
	}


	rg::ImageViewHandle tonemapTargetTextureHandle = g_FXAAEnabled || g_CASEnabled ? finalImageViewHandle : swapchainImageViewHandle;

	// tonemap
	TonemapPass::Data tonemapPassData;
	tonemapPassData.m_passRecordContext = &passRecordContext;
	tonemapPassData.m_bloomEnabled = g_bloomEnabled;
	tonemapPassData.m_bloomStrength = g_bloomStrength;
	tonemapPassData.m_applyLinearToGamma = g_FXAAEnabled || !g_CASEnabled;
	tonemapPassData.m_srcImageHandle = currentOutput;
	tonemapPassData.m_dstImageHandle = tonemapTargetTextureHandle;
	tonemapPassData.m_bloomImageViewHandle = bloomImageViewHandle;
	tonemapPassData.m_avgLuminanceBufferHandle = avgLuminanceBufferViewHandle;
	tonemapPassData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;

	TonemapPass::addToGraph(graph, tonemapPassData);

	currentOutput = tonemapTargetTextureHandle;


	rg::ImageViewHandle fxaaTargetTextureHandle = g_CASEnabled ? finalImageViewHandle2 : swapchainImageViewHandle;

	// FXAA
	FXAAPass::Data fxaaPassData;
	fxaaPassData.m_passRecordContext = &passRecordContext;
	fxaaPassData.m_inputImageHandle = currentOutput;
	fxaaPassData.m_resultImageHandle = fxaaTargetTextureHandle;

	if (g_FXAAEnabled)
	{
		FXAAPass::addToGraph(graph, fxaaPassData);
		currentOutput = fxaaTargetTextureHandle;
	}


	// Sharpen (CAS)
	SharpenFfxCasPass::Data sharpenFfxCasPassData;
	sharpenFfxCasPassData.m_passRecordContext = &passRecordContext;
	sharpenFfxCasPassData.m_gammaSpaceInput = tonemapPassData.m_applyLinearToGamma;
	sharpenFfxCasPassData.m_sharpness = g_CASSharpness;
	sharpenFfxCasPassData.m_srcImageHandle = currentOutput;
	sharpenFfxCasPassData.m_dstImageHandle = swapchainImageViewHandle;

	if (g_CASEnabled)
	{
		SharpenFfxCasPass::addToGraph(graph, sharpenFfxCasPassData);
	}


	// ImGui
	ImGuiPass::Data imGuiPassData;
	imGuiPassData.m_passRecordContext = &passRecordContext;
	imGuiPassData.m_imGuiDrawData = ImGui::GetDrawData();
	imGuiPassData.m_resultImageViewHandle = swapchainImageViewHandle;

	ImGuiPass::addToGraph(graph, imGuiPassData);

	graph.execute(rg::ResourceViewHandle(swapchainImageViewHandle), { {ResourceState::PRESENT_IMAGE, PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT}, m_graphicsDevice->getGraphicsQueue() }, true, m_semaphoreValues[0]);

	auto waitValue = m_semaphoreValues[0];
	auto signalValue = ++m_semaphoreValues[0];
	m_swapChain->present(m_semaphores[0], waitValue, m_semaphores[0], signalValue);

	++m_framesSinceLastResize;
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
		m_volumetricFogModule->resize(width, height);
	}
	m_framesSinceLastResize = 0;
}