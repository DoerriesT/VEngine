#include "Renderer.h"
#include "gal/GraphicsAbstractionLayer.h"
#include "RenderResources.h"
#include "Utility/Utility.h"
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"
#include "TextureLoader.h"
#include "TextureManager.h"
#include "GlobalVar.h"
#include "Pass/IntegrateBrdfPass.h"
#include "Pass/ProbeGBufferPass.h"
#include "Pass/LightProbeGBufferPass.h"
#include "Pass/ProbeDownsamplePass.h"
#include "Pass/ProbeFilterPass.h"
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
#include "Pass/VolumetricFogExtinctionVolumeDebugPass.h"
#include "Pass/FourierOpacityPass.h"
#include "Pass/ParticlesPass.h"
#include "Pass/SwapChainCopyPass.h"
#include "Pass/ApplyIndirectLightingPass.h"
#include "Pass/VolumetricFogApplyPass2.h"
#include "Pass/VisibilityBufferPass.h"
#include "Pass/ShadeVisibilityBufferPass.h"
#include "Pass/ShadeVisibilityBufferPassPS.h"
#include "Pass/FourierOpacityDirectionalLightPass.h"
#include "Pass/DebugDrawPass.h"
#include "Module/GTAOModule.h"
#include "Module/SSRModule.h"
#include "Module/BloomModule.h"
#include "Module/VolumetricFogModule.h"
#include "Module/ReflectionProbeModule.h"
#include "Module/AtmosphericScatteringModule.h"
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

extern bool g_raymarchedFog;

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

	m_swapChainWidth = m_swapChain->getExtent().m_width;
	m_swapChainHeight = m_swapChain->getExtent().m_height;
	m_width = m_swapChainWidth;
	m_height = m_swapChainHeight;

	m_renderResources = new RenderResources(m_graphicsDevice);
	m_renderResources->init(m_width, m_height);

	m_pipelineCache = new PipelineCache(m_graphicsDevice);
	m_descriptorSetCache = new DescriptorSetCache(m_graphicsDevice);
	m_textureLoader = new TextureLoader(m_graphicsDevice, m_renderResources->m_stagingBuffer);
	m_textureManager = new TextureManager(m_graphicsDevice);
	m_materialManager = new MaterialManager(m_graphicsDevice, m_renderResources->m_stagingBuffer, m_renderResources->m_materialBuffer);
	m_meshManager = new MeshManager(m_graphicsDevice,
		m_renderResources->m_stagingBuffer,
		m_renderResources->m_vertexBuffer,
		m_renderResources->m_indexBuffer,
		m_renderResources->m_subMeshDataInfoBuffer,
		m_renderResources->m_subMeshBoundingBoxBuffer,
		m_renderResources->m_subMeshTexCoordScaleBiasBuffer);

	Image *blueNoiseImage;
	ImageView *blueNoiseImageView;
	m_textureLoader->load("Resources/Textures/blue_noise_LDR_RGBA_0.dds", blueNoiseImage, blueNoiseImageView);
	m_blueNoiseTextureIndex = m_textureManager->addTexture2D(blueNoiseImage, blueNoiseImageView);

	m_textureLoader->load("Resources/Textures/blue_noise.dds", m_blueNoiseArrayImage, m_blueNoiseArrayImageView);

	m_editorSceneTextureHandle = m_textureManager->addTexture2D(nullptr, m_renderResources->m_editorSceneTextureView);

	auto imguiFontTextureHandle = m_textureManager->addTexture2D(nullptr, m_renderResources->m_imGuiFontsTextureView);
	ImGui::GetIO().Fonts->SetTexID((ImTextureID)(size_t)imguiFontTextureHandle.m_handle);

	updateTextureData();

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_frameGraphs[i] = new rg::RenderGraph(m_graphicsDevice, m_semaphores, m_semaphoreValues);
	}

	m_gtaoModule = new GTAOModule(m_graphicsDevice, m_width, m_height);
	m_ssrModule = new SSRModule(m_graphicsDevice, m_width, m_height);
	m_volumetricFogModule = new VolumetricFogModule(m_graphicsDevice, m_width, m_height);
	m_reflectionProbeModule = new ReflectionProbeModule(m_graphicsDevice, m_renderResources);
	m_atmosphericScatteringModule = new AtmosphericScatteringModule(m_graphicsDevice);
}

VEngine::Renderer::~Renderer()
{
	m_graphicsDevice->waitIdle();
	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		delete m_frameGraphs[i];
	}
	delete m_pipelineCache;
	delete m_descriptorSetCache;
	delete m_textureLoader;
	delete m_textureManager;
	delete m_materialManager;
	delete m_meshManager;
	delete m_gtaoModule;
	delete m_ssrModule;
	delete m_renderResources;
	delete m_volumetricFogModule;
	delete m_reflectionProbeModule;
	delete m_atmosphericScatteringModule;

	m_graphicsDevice->destroyImageView(m_blueNoiseArrayImageView);
	m_graphicsDevice->destroyImage(m_blueNoiseArrayImage);

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
	rg::ImageViewHandle triangleImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "Triangle Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R8G8B8A8_UNORM;

		triangleImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle triangleDepthBufferImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "Triangle Depth Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::D32_SFLOAT;

		triangleDepthBufferImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}


	rg::ImageViewHandle normalRoughnessImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "Normal/Roughness Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R8G8B8A8_UNORM;

		normalRoughnessImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
	}

	rg::ImageViewHandle albedoMetalnessImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "Albedo/Metalness Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R8G8B8A8_UNORM;

		albedoMetalnessImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 1 } });
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

	rg::ImageViewHandle fomImageViewHandle;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "FOM Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = 2048;
		desc.m_height = 2048;
		desc.m_layers = 2;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R16G16B16A16_SFLOAT;

		fomImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { 0, 1, 0, 2 }, ImageViewType::_2D_ARRAY });
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
	rg::BufferViewHandle localMediaBitMaskBufferViewHandle = ResourceDefinitions::createTiledLightingBitMaskBufferViewHandle(graph, m_width, m_height, static_cast<uint32_t>(lightData.m_localParticipatingMedia.size()));
	rg::BufferViewHandle reflProbeBitMaskBufferViewHandle = ResourceDefinitions::createTiledLightingBitMaskBufferViewHandle(graph, m_width, m_height, static_cast<uint32_t>(lightData.m_localReflectionProbes.size()));
	rg::BufferViewHandle luminanceHistogramBufferViewHandle = ResourceDefinitions::createLuminanceHistogramBufferViewHandle(graph);
	//BufferViewHandle indirectBufferViewHandle = VKResourceDefinitions::createIndirectBufferViewHandle(graph, renderData.m_subMeshInstanceDataCount);
	//BufferViewHandle visibilityBufferViewHandle = VKResourceDefinitions::createOcclusionCullingVisibilityBufferViewHandle(graph, renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount);
	//BufferViewHandle drawCountsBufferViewHandle = VKResourceDefinitions::createIndirectDrawCountsBufferViewHandle(graph, renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount);


	// transform data write
	DescriptorBufferInfo transformDataBufferInfo{ nullptr, 0, std::max(renderData.m_transformDataCount * sizeof(glm::vec4), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(transformDataBufferInfo.m_range, transformDataBufferInfo.m_offset, transformDataBufferInfo.m_buffer, bufferPtr);
		if (renderData.m_transformDataCount)
		{
			memcpy(bufferPtr, renderData.m_transformData, renderData.m_transformDataCount * sizeof(glm::vec4));
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

	// local participating media data write
	DescriptorBufferInfo localMediaDataBufferInfo{ nullptr, 0, std::max(lightData.m_localParticipatingMedia.size() * sizeof(LocalParticipatingMedium), size_t(1)) };
	DescriptorBufferInfo localMediaZBinsBufferInfo{ nullptr, 0, std::max(lightData.m_localMediaDepthBins.size() * sizeof(uint32_t), size_t(1)) };
	{
		uint8_t *dataBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(localMediaDataBufferInfo.m_range, localMediaDataBufferInfo.m_offset, localMediaDataBufferInfo.m_buffer, dataBufferPtr);
		uint8_t *zBinsBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(localMediaZBinsBufferInfo.m_range, localMediaZBinsBufferInfo.m_offset, localMediaZBinsBufferInfo.m_buffer, zBinsBufferPtr);
		if (!lightData.m_localParticipatingMedia.empty())
		{
			for (size_t i = 0; i < lightData.m_localMediaOrder.size(); ++i)
			{
				((LocalParticipatingMedium *)dataBufferPtr)[i] = lightData.m_localParticipatingMedia[lightData.m_localMediaOrder[i]];
			}
			//memcpy(dataBufferPtr, lightData.m_pointLightData.data(), lightData.m_pointLightData.size() * sizeof(PointLightData));
			memcpy(zBinsBufferPtr, lightData.m_localMediaDepthBins.data(), lightData.m_localMediaDepthBins.size() * sizeof(uint32_t));
		}
	}

	// global participating media data write
	DescriptorBufferInfo globalMediaDataBufferInfo{ nullptr, 0, std::max(lightData.m_globalParticipatingMedia.size() * sizeof(GlobalParticipatingMedium), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(globalMediaDataBufferInfo.m_range, globalMediaDataBufferInfo.m_offset, globalMediaDataBufferInfo.m_buffer, bufferPtr);
		if (!lightData.m_globalParticipatingMedia.empty())
		{
			memcpy(bufferPtr, lightData.m_globalParticipatingMedia.data(), lightData.m_globalParticipatingMedia.size() * sizeof(GlobalParticipatingMedium));
		}
	}

	// local reflection probe data write
	DescriptorBufferInfo localReflProbesDataBufferInfo{ nullptr, 0, std::max(lightData.m_localReflectionProbes.size() * sizeof(LocalReflectionProbe), size_t(1)) };
	DescriptorBufferInfo localReflProbesZBinsBufferInfo{ nullptr, 0, std::max(lightData.m_localReflectionProbeDepthBins.size() * sizeof(uint32_t), size_t(1)) };
	{
		uint8_t *dataBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(localReflProbesDataBufferInfo.m_range, localReflProbesDataBufferInfo.m_offset, localReflProbesDataBufferInfo.m_buffer, dataBufferPtr);
		uint8_t *zBinsBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(localReflProbesZBinsBufferInfo.m_range, localReflProbesZBinsBufferInfo.m_offset, localReflProbesZBinsBufferInfo.m_buffer, zBinsBufferPtr);
		if (!lightData.m_localReflectionProbes.empty())
		{
			for (size_t i = 0; i < lightData.m_localReflectionProbeOrder.size(); ++i)
			{
				((LocalReflectionProbe *)dataBufferPtr)[i] = lightData.m_localReflectionProbes[lightData.m_localReflectionProbeOrder[i]];
			}
			memcpy(zBinsBufferPtr, lightData.m_localReflectionProbeDepthBins.data(), lightData.m_localReflectionProbeDepthBins.size() * sizeof(uint32_t));
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

	// particle buffer
	uint32_t totalParticleCount = 0;
	DescriptorBufferInfo particleBufferInfo{ nullptr, 0, 0 };
	{
		for (size_t i = 0; i < renderData.m_particleDataDrawListCount; ++i)
		{
			totalParticleCount += renderData.m_particleDrawDataListSizes[i];
		}

		particleBufferInfo.m_range = std::max(sizeof(ParticleDrawData) * totalParticleCount, size_t(1));

		auto *storageBuffer = m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx].get();

		uint8_t *particleDataPtr = nullptr;
		storageBuffer->allocate(particleBufferInfo.m_range, particleBufferInfo.m_offset, particleBufferInfo.m_buffer, particleDataPtr);

		for (size_t i = 0; i < renderData.m_particleDataDrawListCount; ++i)
		{
			memcpy(particleDataPtr, renderData.m_particleDrawDataLists[i], renderData.m_particleDrawDataListSizes[i] * sizeof(ParticleDrawData));
			particleDataPtr += renderData.m_particleDrawDataListSizes[i] * sizeof(ParticleDrawData);
		}
	}

	PassRecordContext passRecordContext{};
	passRecordContext.m_renderResources = m_renderResources;
	passRecordContext.m_pipelineCache = m_pipelineCache;
	passRecordContext.m_descriptorSetCache = m_descriptorSetCache;
	passRecordContext.m_commonRenderData = &commonData;

	m_atmosphericScatteringModule->registerResources(graph);

	if (!m_startupComputationsDone)
	{
		m_startupComputationsDone = true;

		IntegrateBrdfPass::Data integrateBrdfPassData;
		integrateBrdfPassData.m_passRecordContext = &passRecordContext;
		integrateBrdfPassData.m_resultImageViewHandle = brdfLUTImageViewHandle;

		IntegrateBrdfPass::addToGraph(graph, integrateBrdfPassData);


		AtmosphericScatteringModule::Data atmosphericScatteringModuleData;
		atmosphericScatteringModuleData.m_passRecordContext = &passRecordContext;

		m_atmosphericScatteringModule->addPrecomputationToGraph(graph, atmosphericScatteringModuleData);
	}


	// probe shadow maps
	if (renderData.m_probeRelightCount)
	{
		ReflectionProbeModule::ShadowRenderingData probeShadowPassData;
		probeShadowPassData.m_passRecordContext = &passRecordContext;
		probeShadowPassData.m_renderData = &renderData;
		probeShadowPassData.m_instanceData = sortedInstanceData.data();
		probeShadowPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
		probeShadowPassData.m_directionalLightsShadowedProbe = lightData.m_directionalLightsShadowedProbe.data();
		probeShadowPassData.m_transformDataBufferInfo = transformDataBufferInfo;

		m_reflectionProbeModule->addShadowRenderingToGraph(graph, probeShadowPassData);
	}


	// probe gbuffer pass
	if (renderData.m_probeRenderCount)
	{
		ReflectionProbeModule::GBufferRenderingData probeGBufferPassData;
		probeGBufferPassData.m_passRecordContext = &passRecordContext;
		probeGBufferPassData.m_renderData = &renderData;
		probeGBufferPassData.m_instanceData = sortedInstanceData.data();
		probeGBufferPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
		probeGBufferPassData.m_transformDataBufferInfo = transformDataBufferInfo;

		m_reflectionProbeModule->addGBufferRenderingToGraph(graph, probeGBufferPassData);
	}


	// probe relighting
	if (renderData.m_probeRelightCount)
	{
		ReflectionProbeModule::RelightingData probeRelightPassData;
		probeRelightPassData.m_passRecordContext = &passRecordContext;
		probeRelightPassData.m_relightCount = renderData.m_probeRelightCount;
		probeRelightPassData.m_relightProbeIndices = renderData.m_probeRelightIndices;
		probeRelightPassData.m_directionalLightsBufferInfo = directionalLightsBufferInfo;
		probeRelightPassData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;
		probeRelightPassData.m_lightData = &lightData;

		m_reflectionProbeModule->addRelightingToGraph(graph, probeRelightPassData);
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



	//// visibility buffer
	//VisibilityBufferPass::Data visibilityBufferPassData;
	//visibilityBufferPassData.m_passRecordContext = &passRecordContext;
	//visibilityBufferPassData.m_opaqueInstanceDataCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount;
	//visibilityBufferPassData.m_opaqueInstanceDataOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueOffset;
	//visibilityBufferPassData.m_maskedInstanceDataCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedCount;
	//visibilityBufferPassData.m_maskedInstanceDataOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedOffset;
	//visibilityBufferPassData.m_instanceData = sortedInstanceData.data();
	//visibilityBufferPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
	//visibilityBufferPassData.m_texCoordScaleBias = &renderData.m_texCoordScaleBias[0].x;
	//visibilityBufferPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
	//visibilityBufferPassData.m_transformDataBufferInfo = transformDataBufferInfo;
	//visibilityBufferPassData.m_triangleImageHandle = triangleImageViewHandle;
	//visibilityBufferPassData.m_depthImageHandle = triangleDepthBufferImageViewHandle;
	//
	//VisibilityBufferPass::addToGraph(graph, visibilityBufferPassData);


	// depth prepass
	DepthPrepassPass::Data depthPrePassData;
	depthPrePassData.m_passRecordContext = &passRecordContext;
	depthPrePassData.m_opaqueInstanceDataCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount;
	depthPrePassData.m_opaqueInstanceDataOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueOffset;
	depthPrePassData.m_maskedInstanceDataCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedCount;
	depthPrePassData.m_maskedInstanceDataOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedOffset;
	depthPrePassData.m_instanceData = sortedInstanceData.data();
	depthPrePassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
	depthPrePassData.m_texCoordScaleBias = &renderData.m_texCoordScaleBias[0].x;
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
	rasterTilingPassData.m_participatingMediaBitMaskBufferHandle = localMediaBitMaskBufferViewHandle;
	rasterTilingPassData.m_reflectionProbeBitMaskBufferHandle = reflProbeBitMaskBufferViewHandle;

	if (!lightData.m_punctualLights.empty() || !lightData.m_punctualLightsShadowed.empty() || !lightData.m_localParticipatingMedia.empty() || !lightData.m_localReflectionProbes.empty())
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
	shadowAtlasPassData.m_texCoordScaleBias = &renderData.m_texCoordScaleBias[0].x;
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
			shadowPassData.m_shadowMatrix = renderData.m_shadowMatrices[lightData.m_directionalLightsShadowed[0].m_shadowOffset + i];
			shadowPassData.m_opaqueInstanceDataCount = drawList.m_opaqueCount;
			shadowPassData.m_opaqueInstanceDataOffset = drawList.m_opaqueOffset;
			shadowPassData.m_maskedInstanceDataCount = drawList.m_maskedCount;
			shadowPassData.m_maskedInstanceDataOffset = drawList.m_maskedOffset;
			shadowPassData.m_instanceData = sortedInstanceData.data();
			shadowPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
			shadowPassData.m_texCoordScaleBias = &renderData.m_texCoordScaleBias[0].x;
			shadowPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
			shadowPassData.m_transformDataBufferInfo = transformDataBufferInfo;
			shadowPassData.m_shadowImageHandle = shadowLayer;

			ShadowPass::addToGraph(graph, shadowPassData);
		}
	}

	rg::ImageViewHandle fomDirShadowImageViewHandle = 0;
	{
		rg::ImageDescription desc = {};
		desc.m_name = "FOM Cascades Image";
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = 256;
		desc.m_height = 256;
		desc.m_layers = glm::max(renderData.m_shadowCascadeViewRenderListCount, 1u) * 2;
		desc.m_levels = 1;
		desc.m_samples = SampleCount::_1;
		desc.m_format = Format::R16G16B16A16_SFLOAT;

		rg::ImageHandle imageHandle = graph.createImage(desc);
		fomDirShadowImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { 0, 1, 0, desc.m_layers }, ImageViewType::_2D_ARRAY });

		if (renderData.m_shadowCascadeViewRenderListCount > 0)
		{
			FourierOpacityDirectionalLightPass::Data fourierOpacityDirLightPassData;
			fourierOpacityDirLightPassData.m_passRecordContext = &passRecordContext;
			fourierOpacityDirLightPassData.m_lightDataCount = static_cast<uint32_t>(lightData.m_directionalLightsShadowed.size());
			fourierOpacityDirLightPassData.m_lightData = lightData.m_directionalLightsShadowed.data();
			fourierOpacityDirLightPassData.m_listCount = renderData.m_particleDataDrawListCount;
			fourierOpacityDirLightPassData.m_particleLists = renderData.m_particleDrawDataLists;
			fourierOpacityDirLightPassData.m_listSizes = renderData.m_particleDrawDataListSizes;
			fourierOpacityDirLightPassData.m_localMediaBufferInfo = localMediaDataBufferInfo;
			fourierOpacityDirLightPassData.m_particleBufferInfo = particleBufferInfo;
			fourierOpacityDirLightPassData.m_shadowMatrixBufferInfo = shadowMatricesBufferInfo;
			fourierOpacityDirLightPassData.m_directionalLightFomImageHandle = imageHandle;

			FourierOpacityDirectionalLightPass::addToGraph(graph, fourierOpacityDirLightPassData);
		}
	}
	


	// fourier opacity
	if (!lightData.m_fomAtlasDrawInfos.empty())
	{
		FourierOpacityPass::Data fourierOpacityPassData;
		fourierOpacityPassData.m_passRecordContext = &passRecordContext;
		fourierOpacityPassData.m_drawCount = static_cast<uint32_t>(lightData.m_fomAtlasDrawInfos.size());
		fourierOpacityPassData.m_particleCount = totalParticleCount;
		fourierOpacityPassData.m_drawInfo = lightData.m_fomAtlasDrawInfos.data();
		fourierOpacityPassData.m_localMediaBufferInfo = localMediaDataBufferInfo;
		fourierOpacityPassData.m_globalMediaBufferInfo = globalMediaDataBufferInfo;
		fourierOpacityPassData.m_particleBufferInfo = particleBufferInfo;
		fourierOpacityPassData.m_fomImageViewHandle = fomImageViewHandle;

		FourierOpacityPass::addToGraph(graph, fourierOpacityPassData);
	}


	// deferred shadows
	DeferredShadowsPass::Data deferredShadowsPassData;
	deferredShadowsPassData.m_passRecordContext = &passRecordContext;
	deferredShadowsPassData.m_lightDataCount = static_cast<uint32_t>(lightData.m_directionalLightsShadowed.size());
	deferredShadowsPassData.m_lightData = lightData.m_directionalLightsShadowed.data();
	deferredShadowsPassData.m_blueNoiseImageView = m_blueNoiseArrayImageView;
	deferredShadowsPassData.m_resultImageHandle = deferredShadowsImageHandle;
	deferredShadowsPassData.m_depthImageViewHandle = depthImageViewHandle;
	deferredShadowsPassData.m_directionalLightFOMImageViewHandle = fomDirShadowImageViewHandle;
	//deferredShadowsPassData.m_tangentSpaceImageViewHandle = tangentSpaceImageViewHandle;
	deferredShadowsPassData.m_shadowImageViewHandle = shadowImageViewHandle;
	deferredShadowsPassData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;
	deferredShadowsPassData.m_cascadeParamsBufferInfo = shadowCascadeParamsBufferInfo;

	DeferredShadowsPass::addToGraph(graph, deferredShadowsPassData);






	// volumetric fog
	VolumetricFogModule::Data volumetricFogModuleData;
	volumetricFogModuleData.m_passRecordContext = &passRecordContext;
	volumetricFogModuleData.m_ignoreHistory = m_framesSinceLastResize < RendererConsts::FRAMES_IN_FLIGHT;
	volumetricFogModuleData.m_commonData = &commonData;
	volumetricFogModuleData.m_blueNoiseImageView = m_blueNoiseArrayImageView;
	volumetricFogModuleData.m_shadowImageViewHandle = shadowImageViewHandle;
	volumetricFogModuleData.m_shadowAtlasImageViewHandle = shadowAtlasImageViewHandle;
	volumetricFogModuleData.m_fomImageViewHandle = fomImageViewHandle;
	volumetricFogModuleData.m_depthImageViewHandle = depthImageViewHandle;
	volumetricFogModuleData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	volumetricFogModuleData.m_punctualLightsBitMaskBufferHandle = punctualLightBitMaskBufferViewHandle;
	volumetricFogModuleData.m_punctualLightsShadowedBitMaskBufferHandle = punctualLightShadowedBitMaskBufferViewHandle;
	volumetricFogModuleData.m_localMediaBitMaskBufferHandle = localMediaBitMaskBufferViewHandle;
	volumetricFogModuleData.m_directionalLightsBufferInfo = directionalLightsBufferInfo;
	volumetricFogModuleData.m_directionalLightsShadowedBufferInfo = directionalLightsShadowedBufferInfo;
	volumetricFogModuleData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;
	volumetricFogModuleData.m_punctualLightsBufferInfo = punctualLightDataBufferInfo;
	volumetricFogModuleData.m_punctualLightsZBinsBufferInfo = punctualLightZBinsBufferInfo;
	volumetricFogModuleData.m_punctualLightsShadowedBufferInfo = punctualLightShadowedDataBufferInfo;
	volumetricFogModuleData.m_punctualLightsShadowedZBinsBufferInfo = punctualLightShadowedZBinsBufferInfo;
	volumetricFogModuleData.m_localMediaBufferInfo = localMediaDataBufferInfo;
	volumetricFogModuleData.m_localMediaZBinsBufferInfo = localMediaZBinsBufferInfo;
	volumetricFogModuleData.m_globalMediaBufferInfo = globalMediaDataBufferInfo;

	m_volumetricFogModule->addToGraph(graph, volumetricFogModuleData);


	// forward lighting
	ForwardLightingPass::Data forwardPassData;
	forwardPassData.m_passRecordContext = &passRecordContext;
	forwardPassData.m_instanceDataCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount
		+ renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedCount; // masked entities come right after opaque entities in the list, so we can just add the counts
	forwardPassData.m_instanceDataOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueOffset;
	forwardPassData.m_instanceData = sortedInstanceData.data();
	forwardPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
	forwardPassData.m_texCoordScaleBias = &renderData.m_texCoordScaleBias[0].x;
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
	forwardPassData.m_normalRoughnessImageViewHandle = normalRoughnessImageViewHandle;
	forwardPassData.m_albedoMetalnessImageViewHandle = albedoMetalnessImageViewHandle;
	//forwardPassData.m_volumetricFogImageViewHandle = m_volumetricFogModule->getVolumetricScatteringImageViewHandle();
	//forwardPassData.m_ssaoImageViewHandle = m_gtaoModule->getAOResultImageViewHandle(); // TODO: what to pass in when ssao is disabled?
	forwardPassData.m_shadowAtlasImageViewHandle = shadowAtlasImageViewHandle;
	forwardPassData.m_probeImageView = m_reflectionProbeModule->getCubeArrayView();
	forwardPassData.m_atmosphereConstantBufferInfo = m_atmosphericScatteringModule->getConstantBufferInfo();
	forwardPassData.m_atmosphereScatteringImageViewHandle = m_atmosphericScatteringModule->getScatteringImageViewHandle();
	forwardPassData.m_atmosphereTransmittanceImageViewHandle = m_atmosphericScatteringModule->getTransmittanceImageViewHandle();
	forwardPassData.m_extinctionVolumeImageViewHandle = m_volumetricFogModule->getExtinctionVolumeImageViewHandle();
	forwardPassData.m_fomImageViewHandle = fomImageViewHandle;

	ForwardLightingPass::addToGraph(graph, forwardPassData);


	//// shade visibility buffer
	//ShadeVisibilityBufferPass::Data shadeVisBufferPassData;
	//shadeVisBufferPassData.m_passRecordContext = &passRecordContext;
	//shadeVisBufferPassData.m_directionalLightsBufferInfo = directionalLightsBufferInfo;
	//shadeVisBufferPassData.m_directionalLightsShadowedBufferInfo = directionalLightsShadowedBufferInfo;
	//shadeVisBufferPassData.m_punctualLightsBufferInfo = punctualLightDataBufferInfo;
	//shadeVisBufferPassData.m_punctualLightsZBinsBufferInfo = punctualLightZBinsBufferInfo;
	//shadeVisBufferPassData.m_punctualLightsShadowedBufferInfo = punctualLightShadowedDataBufferInfo;
	//shadeVisBufferPassData.m_punctualLightsShadowedZBinsBufferInfo = punctualLightShadowedZBinsBufferInfo;
	//shadeVisBufferPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
	//shadeVisBufferPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	//shadeVisBufferPassData.m_transformDataBufferInfo = transformDataBufferInfo;
	//shadeVisBufferPassData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer, 0, m_renderResources->m_subMeshDataInfoBuffer->getDescription().m_size };
	//shadeVisBufferPassData.m_indexBufferInfo = { m_renderResources->m_indexBuffer, 0, m_renderResources->m_indexBuffer->getDescription().m_size };
	//shadeVisBufferPassData.m_punctualLightsBitMaskBufferHandle = punctualLightBitMaskBufferViewHandle;
	//shadeVisBufferPassData.m_punctualLightsShadowedBitMaskBufferHandle = punctualLightShadowedBitMaskBufferViewHandle;
	//shadeVisBufferPassData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	//shadeVisBufferPassData.m_deferredShadowImageViewHandle = deferredShadowsImageViewHandle;
	//shadeVisBufferPassData.m_resultImageViewHandle = lightImageViewHandle;
	//shadeVisBufferPassData.m_normalRoughnessImageViewHandle = normalRoughnessImageViewHandle;
	//shadeVisBufferPassData.m_albedoMetalnessImageViewHandle = albedoMetalnessImageViewHandle;
	//shadeVisBufferPassData.m_shadowAtlasImageViewHandle = shadowAtlasImageViewHandle;
	//shadeVisBufferPassData.m_fomImageViewHandle = fomImageViewHandle;
	//shadeVisBufferPassData.m_triangleImageViewHandle = triangleImageViewHandle;
	//
	//ShadeVisibilityBufferPass::addToGraph(graph, shadeVisBufferPassData);


	//// shade visibility buffer PS
	//ShadeVisibilityBufferPassPS::Data shadeVisBufferPassPSData;
	//shadeVisBufferPassPSData.m_passRecordContext = &passRecordContext;
	//shadeVisBufferPassPSData.m_directionalLightsBufferInfo = directionalLightsBufferInfo;
	//shadeVisBufferPassPSData.m_directionalLightsShadowedBufferInfo = directionalLightsShadowedBufferInfo;
	//shadeVisBufferPassPSData.m_punctualLightsBufferInfo = punctualLightDataBufferInfo;
	//shadeVisBufferPassPSData.m_punctualLightsZBinsBufferInfo = punctualLightZBinsBufferInfo;
	//shadeVisBufferPassPSData.m_punctualLightsShadowedBufferInfo = punctualLightShadowedDataBufferInfo;
	//shadeVisBufferPassPSData.m_punctualLightsShadowedZBinsBufferInfo = punctualLightShadowedZBinsBufferInfo;
	//shadeVisBufferPassPSData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer, 0, m_renderResources->m_materialBuffer->getDescription().m_size };
	//shadeVisBufferPassPSData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	//shadeVisBufferPassPSData.m_transformDataBufferInfo = transformDataBufferInfo;
	//shadeVisBufferPassPSData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer, 0, m_renderResources->m_subMeshDataInfoBuffer->getDescription().m_size };
	//shadeVisBufferPassPSData.m_indexBufferInfo = { m_renderResources->m_indexBuffer, 0, m_renderResources->m_indexBuffer->getDescription().m_size };
	//shadeVisBufferPassPSData.m_punctualLightsBitMaskBufferHandle = punctualLightBitMaskBufferViewHandle;
	//shadeVisBufferPassPSData.m_punctualLightsShadowedBitMaskBufferHandle = punctualLightShadowedBitMaskBufferViewHandle;
	//shadeVisBufferPassPSData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	//shadeVisBufferPassPSData.m_deferredShadowImageViewHandle = deferredShadowsImageViewHandle;
	//shadeVisBufferPassPSData.m_resultImageViewHandle = lightImageViewHandle;
	//shadeVisBufferPassPSData.m_normalRoughnessImageViewHandle = normalRoughnessImageViewHandle;
	//shadeVisBufferPassPSData.m_albedoMetalnessImageViewHandle = albedoMetalnessImageViewHandle;
	//shadeVisBufferPassPSData.m_shadowAtlasImageViewHandle = shadowAtlasImageViewHandle;
	//shadeVisBufferPassPSData.m_fomImageViewHandle = fomImageViewHandle;
	//shadeVisBufferPassPSData.m_triangleImageViewHandle = triangleImageViewHandle;
	//shadeVisBufferPassPSData.m_depthImageViewHandle = triangleDepthBufferImageViewHandle;
	//
	//ShadeVisibilityBufferPassPS::addToGraph(graph, shadeVisBufferPassPSData);


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
	//SSRModule::Data ssrModuleData;
	//ssrModuleData.m_passRecordContext = &passRecordContext;
	//ssrModuleData.m_ignoreHistory = m_framesSinceLastResize < RendererConsts::FRAMES_IN_FLIGHT;
	//ssrModuleData.m_noiseTextureHandle = m_blueNoiseTextureIndex.m_handle;
	//ssrModuleData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	//ssrModuleData.m_hiZPyramidImageViewHandle = hiZMaxPyramidPassOutData.m_resultImageViewHandle;
	//ssrModuleData.m_normalRoughnessImageViewHandle = normalRoughnessImageViewHandle;
	//ssrModuleData.m_depthImageViewHandle = depthImageViewHandle;
	//ssrModuleData.m_albedoMetalnessImageViewHandle = albedoMetalnessImageViewHandle; 
	//ssrModuleData.m_prevColorImageViewHandle = prevLightImageViewHandle;
	//ssrModuleData.m_velocityImageViewHandle = velocityImageViewHandle;
	//
	//m_ssrModule->addToGraph(graph, ssrModuleData);


	// gtao
	GTAOModule::Data gtaoModuleData;
	gtaoModuleData.m_passRecordContext = &passRecordContext;
	gtaoModuleData.m_ignoreHistory = m_framesSinceLastResize < RendererConsts::FRAMES_IN_FLIGHT;
	gtaoModuleData.m_depthImageViewHandle = depthImageViewHandle;
	gtaoModuleData.m_normalImageViewHandle = normalRoughnessImageViewHandle;
	gtaoModuleData.m_velocityImageViewHandle = velocityImageViewHandle;

	if (g_ssaoEnabled)
	{
		m_gtaoModule->addToGraph(graph, gtaoModuleData);
	}


	// apply indirect lighting
	ApplyIndirectLightingPass::Data indirectLightingApplyPassData;
	indirectLightingApplyPassData.m_passRecordContext = &passRecordContext;
	indirectLightingApplyPassData.m_ssao = g_ssaoEnabled;
	indirectLightingApplyPassData.m_reflectionProbeDataBufferInfo = localReflProbesDataBufferInfo;
	indirectLightingApplyPassData.m_reflectionProbeZBinsBufferInfo = localReflProbesZBinsBufferInfo;
	indirectLightingApplyPassData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	indirectLightingApplyPassData.m_reflectionProbeImageView = m_reflectionProbeModule->getCubeArrayView();
	indirectLightingApplyPassData.m_reflectionProbeBitMaskBufferHandle = reflProbeBitMaskBufferViewHandle;
	indirectLightingApplyPassData.m_depthImageViewHandle = depthImageViewHandle;
	indirectLightingApplyPassData.m_depthImageViewHandle2 = hiZMaxPyramidPassOutData.m_resultImageViewHandle;
	//indirectLightingApplyPassData.m_indirectSpecularLightImageViewHandle = m_ssrModule->getSSRResultImageViewHandle();
	indirectLightingApplyPassData.m_brdfLutImageViewHandle = brdfLUTImageViewHandle;
	indirectLightingApplyPassData.m_albedoMetalnessImageViewHandle = albedoMetalnessImageViewHandle;
	indirectLightingApplyPassData.m_normalRoughnessImageViewHandle = normalRoughnessImageViewHandle;
	indirectLightingApplyPassData.m_ssaoImageViewHandle = m_gtaoModule->getAOResultImageViewHandle(); // TODO: what to pass in when ssao is disabled?
	indirectLightingApplyPassData.m_resultImageHandle = lightImageViewHandle;

	ApplyIndirectLightingPass::addToGraph(graph, indirectLightingApplyPassData);


	// apply volumetric fog
	VolumetricFogApplyPass2::Data volumetricFogApplyPass2Data;
	volumetricFogApplyPass2Data.m_passRecordContext = &passRecordContext;
	volumetricFogApplyPass2Data.m_blueNoiseImageView = m_blueNoiseArrayImageView;
	volumetricFogApplyPass2Data.m_depthImageViewHandle = depthImageViewHandle;
	volumetricFogApplyPass2Data.m_downsampledDepthImageViewHandle = m_volumetricFogModule->getDownsampledDepthImageViewHandle();
	volumetricFogApplyPass2Data.m_volumetricFogImageViewHandle = m_volumetricFogModule->getVolumetricScatteringImageViewHandle();
	volumetricFogApplyPass2Data.m_raymarchedVolumetricsImageViewHandle = m_volumetricFogModule->getRaymarchedScatteringImageViewHandle();
	volumetricFogApplyPass2Data.m_resultImageHandle = lightImageViewHandle;

	VolumetricFogApplyPass2::addToGraph(graph, volumetricFogApplyPass2Data);



	//// apply volumetric fog and indirect specular light to scene
	//VolumetricFogApplyPass::Data volumetricFogApplyPassData;
	//volumetricFogApplyPassData.m_passRecordContext = &passRecordContext;
	//volumetricFogApplyPassData.m_ssao = g_ssaoEnabled;
	//volumetricFogApplyPassData.m_reflectionProbeDataBufferInfo = localReflProbesDataBufferInfo;
	//volumetricFogApplyPassData.m_reflectionProbeZBinsBufferInfo = localReflProbesZBinsBufferInfo;
	//volumetricFogApplyPassData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	//volumetricFogApplyPassData.m_reflectionProbeImageView = m_reflectionProbeModule->getCubeArrayView();
	//volumetricFogApplyPassData.m_reflectionProbeBitMaskBufferHandle = reflProbeBitMaskBufferViewHandle;
	//volumetricFogApplyPassData.m_blueNoiseImageView = m_blueNoiseArrayImageView;
	//volumetricFogApplyPassData.m_depthImageViewHandle = depthImageViewHandle;
	//volumetricFogApplyPassData.m_volumetricFogImageViewHandle = m_volumetricFogModule->getVolumetricScatteringImageViewHandle();
	////volumetricFogApplyPassData.m_indirectSpecularLightImageViewHandle = m_ssrModule->getSSRResultImageViewHandle();
	//volumetricFogApplyPassData.m_brdfLutImageViewHandle = brdfLUTImageViewHandle;
	//volumetricFogApplyPassData.m_albedoMetalnessImageViewHandle = albedoMetalnessImageViewHandle;
	//volumetricFogApplyPassData.m_normalRoughnessImageViewHandle = normalRoughnessImageViewHandle;
	//volumetricFogApplyPassData.m_ssaoImageViewHandle = m_gtaoModule->getAOResultImageViewHandle(); // TODO: what to pass in when ssao is disabled?
	//volumetricFogApplyPassData.m_raymarchedVolumetricsImageViewHandle = m_volumetricFogModule->getRaymarchedScatteringImageViewHandle();
	//volumetricFogApplyPassData.m_resultImageHandle = lightImageViewHandle;
	//
	//VolumetricFogApplyPass::addToGraph(graph, volumetricFogApplyPassData);


	ParticlesPass::Data particlesPassData;
	particlesPassData.m_passRecordContext = &passRecordContext;
	particlesPassData.m_listCount = renderData.m_particleDataDrawListCount;
	particlesPassData.m_particleLists = renderData.m_particleDrawDataLists;
	particlesPassData.m_listSizes = renderData.m_particleDrawDataListSizes;
	particlesPassData.m_blueNoiseImageView = m_blueNoiseArrayImageView;
	particlesPassData.m_volumetricFogImageViewHandle = m_volumetricFogModule->getVolumetricScatteringImageViewHandle();
	particlesPassData.m_shadowImageViewHandle = shadowImageViewHandle;
	particlesPassData.m_shadowAtlasImageViewHandle = shadowAtlasImageViewHandle;
	particlesPassData.m_fomImageViewHandle = fomImageViewHandle;
	particlesPassData.m_exposureDataBufferHandle = exposureDataBufferViewHandle;
	particlesPassData.m_punctualLightsBitMaskBufferHandle = punctualLightBitMaskBufferViewHandle;
	particlesPassData.m_punctualLightsShadowedBitMaskBufferHandle = punctualLightShadowedBitMaskBufferViewHandle;
	particlesPassData.m_directionalLightsBufferInfo = directionalLightsBufferInfo;
	particlesPassData.m_directionalLightsShadowedBufferInfo = directionalLightsShadowedBufferInfo;
	particlesPassData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;
	particlesPassData.m_punctualLightsBufferInfo = punctualLightDataBufferInfo;
	particlesPassData.m_punctualLightsZBinsBufferInfo = punctualLightZBinsBufferInfo;
	particlesPassData.m_punctualLightsShadowedBufferInfo = punctualLightShadowedDataBufferInfo;
	particlesPassData.m_punctualLightsShadowedZBinsBufferInfo = punctualLightShadowedZBinsBufferInfo;
	particlesPassData.m_particleBufferInfo = particleBufferInfo;
	particlesPassData.m_depthImageViewHandle = depthImageViewHandle;
	particlesPassData.m_resultImageViewHandle = lightImageViewHandle;

	if (renderData.m_particleDataDrawListCount)
	{
		ParticlesPass::addToGraph(graph, particlesPassData);
	}


	//VolumetricFogExtinctionVolumeDebugPass::Data extinctionVolumeDebugData;
	//extinctionVolumeDebugData.m_passRecordContext = &passRecordContext;
	//extinctionVolumeDebugData.m_extinctionVolumeImageViewHandle = m_volumetricFogModule->getExtinctionVolumeImageViewHandle();
	//extinctionVolumeDebugData.m_resultImageViewHandle = lightImageViewHandle;
	//
	//VolumetricFogExtinctionVolumeDebugPass::addToGraph(graph, extinctionVolumeDebugData);


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


	DebugDrawPass::Data debugDrawPassData;
	debugDrawPassData.m_passRecordContext = &passRecordContext;
	debugDrawPassData.m_debugDrawData = renderData.m_debugDrawData;
	debugDrawPassData.m_depthImageViewHandle = depthImageViewHandle;
	debugDrawPassData.m_resultImageViewHandle = swapchainImageViewHandle;

	DebugDrawPass::addToGraph(graph, debugDrawPassData);


	// ImGui
	ImGuiPass::Data imGuiPassData;
	imGuiPassData.m_passRecordContext = &passRecordContext;
	imGuiPassData.m_clear = false;
	imGuiPassData.m_imGuiDrawData = ImGui::GetDrawData();
	imGuiPassData.m_resultImageViewHandle = swapchainImageViewHandle;

	ImGuiPass::addToGraph(graph, imGuiPassData);


	// render editor
	if (m_editorMode)
	{
		// copy result from swapchain to a texture that can be read by imgui
		SwapChainCopyPass::Data swapChainCopyPassData;
		swapChainCopyPassData.m_passRecordContext = &passRecordContext;
		swapChainCopyPassData.m_inputImageViewHandle = swapchainImageViewHandle;
		swapChainCopyPassData.m_resultImageView = m_renderResources->m_editorSceneTextureView;

		SwapChainCopyPass::addToGraph(graph, swapChainCopyPassData);


		// switch contexts
		auto *prevImGuiContext = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext(m_editorImGuiContext);


		// draw editor gui
		ImGuiPass::Data editorImGuiPassData;
		editorImGuiPassData.m_passRecordContext = &passRecordContext;
		editorImGuiPassData.m_clear = true;
		editorImGuiPassData.m_imGuiDrawData = ImGui::GetDrawData();
		editorImGuiPassData.m_resultImageViewHandle = swapchainImageViewHandle;

		ImGuiPass::addToGraph(graph, editorImGuiPassData);


		// switch contexts back to main context
		ImGui::SetCurrentContext(prevImGuiContext);
	}

	graph.execute(rg::ResourceViewHandle(swapchainImageViewHandle), { {ResourceState::PRESENT_IMAGE, PipelineStageFlagBits::BOTTOM_OF_PIPE_BIT}, m_graphicsDevice->getGraphicsQueue() }, true, m_semaphoreValues[0]);

	auto waitValue = m_semaphoreValues[0];
	auto signalValue = ++m_semaphoreValues[0];
	m_swapChain->present(m_semaphores[0], waitValue, m_semaphores[0], signalValue);

	++m_framesSinceLastResize;
}

VEngine::Texture2DHandle VEngine::Renderer::loadTexture(const char *filepath)
{
	Image *image;
	ImageView *imageView;
	m_textureLoader->load(filepath, image, imageView);
	return m_textureManager->addTexture2D(image, imageView);
}

VEngine::Texture3DHandle VEngine::Renderer::loadTexture3D(const char *filepath)
{
	Image *image;
	ImageView *imageView;
	m_textureLoader->load(filepath, image, imageView);
	return m_textureManager->addTexture3D(image, imageView);
}

void VEngine::Renderer::freeTexture(Texture2DHandle handle)
{
	m_textureManager->free(handle);
}

void VEngine::Renderer::freeTexture(Texture3DHandle handle)
{
	m_textureManager->free(handle);
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
	m_renderResources->updateTextureArray(RendererConsts::TEXTURE_ARRAY_SIZE, m_textureManager->get2DViews());
}

void VEngine::Renderer::updateTexture3DData()
{
	m_renderResources->updateTexture3DArray(RendererConsts::TEXTURE_ARRAY_SIZE, m_textureManager->get3DViews());
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
	resize(width, height, width, height);
}

void VEngine::Renderer::resize(uint32_t width, uint32_t height, uint32_t swapChainWidth, uint32_t swapChainHeight)
{
	if (!m_editorMode)
	{
		width = swapChainWidth;
		height = swapChainHeight;
	}
	if (width != m_width || height != m_height || swapChainWidth != m_swapChainWidth || swapChainHeight != m_swapChainHeight)
	{
		m_graphicsDevice->waitIdle();
		if (width > 0 && height > 0 && swapChainWidth > 0 && swapChainHeight > 0)
		{
			for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
			{
				m_frameGraphs[i]->reset();
			}
			if (swapChainWidth != m_swapChainWidth || swapChainHeight != m_swapChainHeight)
			{
				m_swapChain->resize(swapChainWidth, swapChainHeight);
			}
			if (width != m_width || height != m_height)
			{
				m_renderResources->resize(width, height);
				m_gtaoModule->resize(width, height);
				m_ssrModule->resize(width, height);
				m_volumetricFogModule->resize(width, height);
				m_textureManager->update(m_editorSceneTextureHandle, nullptr, m_renderResources->m_editorSceneTextureView);
				updateTextureData();
			}
		}
		m_framesSinceLastResize = 0;

		m_width = width;
		m_height = height;
		m_swapChainWidth = swapChainWidth;
		m_swapChainHeight = swapChainHeight;
	}
}

void VEngine::Renderer::setEditorMode(bool editorMode)
{
	m_editorMode = editorMode;
}

void VEngine::Renderer::initEditorImGuiCtx(ImGuiContext *editorImGuiCtx)
{
	if (!m_editorImGuiContext)
	{
		// get font texture of main context
		auto fontTexture = ImGui::GetIO().Fonts->TexID;

		// switch contexts
		auto *prevImGuiContext = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext(editorImGuiCtx);

		// set font texture of editor context (we dont create an additional texture, so we just ignore the pixels return value)
		unsigned char *pixels;
		ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, nullptr, nullptr);
		ImGui::GetIO().Fonts->SetTexID(fontTexture);

		// switch contexts back to main context
		ImGui::SetCurrentContext(prevImGuiContext);

		m_editorImGuiContext = editorImGuiCtx;
	}
}

VEngine::Texture2DHandle VEngine::Renderer::getEditorSceneTextureHandle()
{
	return m_editorSceneTextureHandle;
}
