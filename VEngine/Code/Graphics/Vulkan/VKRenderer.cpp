#include "VKRenderer.h"
#include "SwapChainVK.h"
#include "VKRenderResources.h"
#include "Utility/Utility.h"
#include "VKUtility.h"
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"
#include "VKTextureLoader.h"
#include "GlobalVar.h"
#include "Pass/IntegrateBrdfPass.h"
#include "Pass/VKGeometryPass.h"
#include "Pass/VKShadowPass.h"
#include "Pass/VKRasterTilingPass.h"
#include "Pass/VKLuminanceHistogramPass.h"
#include "Pass/VKLuminanceHistogramReduceAveragePass.h"
#include "Pass/VKTonemapPass.h"
#include "Pass/VKTAAResolvePass.h"
#include "Pass/VKVelocityInitializationPass.h"
#include "Pass/VKFXAAPass.h"
#include "Pass/LightingPass.h"
#include "Pass/DeferredShadowsPass.h"
#include "Pass/ImGuiPass.h"
#include "Pass/ReadBackCopyPass.h"
#include "Pass/MeshClusterVisualizationPass.h"
#include "Pass/OcclusionCullingReprojectionPass.h"
#include "Pass/OcclusionCullingCopyToDepthPass.h"
#include "Pass/OcclusionCullingPass.h"
#include "Pass/OcclusionCullingCreateDrawArgsPass.h"
#include "Pass/OcclusionCullingHiZPass.h"
#include "Pass/HiZPyramidPass.h"
#include "Pass/BuildIndexBufferPass.h"
#include "Pass/RayTraceTestPass.h"
#include "Pass/SharpenFfxCasPass.h"
#include "Pass/IndirectDiffusePass.h"
#include "Pass/IndirectLightPass.h"
#include "Pass/TAAPass.h"
#include "Pass/GenerateMipMapsPass.h"
#include "Module/GTAOModule.h"
#include "Module/SSRModule.h"
#include "Module/SparseVoxelBricksModule.h"
#include "Module/DiffuseGIProbesModule.h"
#include "Module/BloomModule.h"
#include "VKPipelineCache.h"
#include "VKDescriptorSetCache.h"
#include "VKMaterialManager.h"
#include "VKMeshManager.h"
#include "VKResourceDefinitions.h"
#include <glm/gtc/matrix_transform.hpp>
#include "RenderGraph.h"
#include "PassRecordContext.h"
#include "RenderPassCache.h"
#include "DeferredObjectDeleter.h"
#include "Graphics/imgui/imgui.h"
#include "Graphics/ViewRenderList.h"

extern uint32_t g_debugVoxelCascadeIndex;
extern uint32_t g_giVoxelDebugMode;
extern uint32_t g_allocatedBricks;

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
	m_meshManager = std::make_unique<VKMeshManager>(m_renderResources->m_stagingBuffer, m_renderResources->m_vertexBuffer, m_renderResources->m_indexBuffer, m_renderResources->m_subMeshDataInfoBuffer, m_renderResources->m_subMeshBoundingBoxBuffer);
	m_swapChain = std::make_unique<SwapChainVK>(g_context.m_physicalDevice, g_context.m_device, g_context.m_surface, width, height, g_context.m_graphicsQueue);
	m_width = m_swapChain->getExtent().width;
	m_height = m_swapChain->getExtent().height;
	m_renderResources->init(m_width, m_height);

	m_fontAtlasTextureIndex = m_textureLoader->load("Resources/Textures/fontConsolas.dds");
	m_blueNoiseTextureIndex = m_textureLoader->load("Resources/Textures/blue_noise_LDR_RGBA_0.dds");

	updateTextureData();

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		m_frameGraphs[i] = std::make_unique<RenderGraph>();
	}

	m_gtaoModule = std::make_unique<GTAOModule>(m_width, m_height);
	m_ssrModule = std::make_unique<SSRModule>(m_width, m_height);
	m_sparseVoxelBricksModule = std::make_unique<SparseVoxelBricksModule>();
	m_diffuseGIProbesModule = std::make_unique<DiffuseGIProbesModule>();
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
	if (m_width == 0 || m_height == 0)
	{
		return;
	}
	auto &graph = *m_frameGraphs[commonData.m_curResIdx];

	// reset per frame resources
	graph.reset();
	m_renderResources->m_mappableUBOBlock[commonData.m_curResIdx]->reset();
	m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->reset();
	m_descriptorSetCache->update(commonData.m_frame, commonData.m_frame - RendererConsts::FRAMES_IN_FLIGHT);
	m_deferredObjectDeleter->update(commonData.m_frame, commonData.m_frame - RendererConsts::FRAMES_IN_FLIGHT);

	// read back luminance histogram
	{
		auto &buffer = m_renderResources->m_luminanceHistogramReadBackBuffers[commonData.m_curResIdx];
		uint32_t *data;
		g_context.m_allocator.mapMemory(buffer.getAllocation(), (void **)&data);

		VkMappedMemoryRange range{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		range.memory = buffer.getDeviceMemory();
		range.offset = Utility::alignDown(buffer.getOffset(), g_context.m_properties.limits.nonCoherentAtomSize);
		range.size = Utility::alignUp(buffer.getSize(), g_context.m_properties.limits.nonCoherentAtomSize);

		vkInvalidateMappedMemoryRanges(g_context.m_device, 1, &range);
		memcpy(m_luminanceHistogram, data, sizeof(m_luminanceHistogram));

		g_context.m_allocator.unmapMemory(buffer.getAllocation());
	}

	// read back occlusion cull stats
	//{
	//	auto &buffer = m_renderResources->m_occlusionCullStatsReadBackBuffers[commonData.m_curResIdx];
	//	uint32_t *data;
	//	g_context.m_allocator.mapMemory(buffer.getAllocation(), (void **)&data);
	//
	//	VkMappedMemoryRange range{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
	//	range.memory = buffer.getDeviceMemory();
	//	range.offset = Utility::alignDown(buffer.getOffset(), g_context.m_properties.limits.nonCoherentAtomSize);
	//	range.size = Utility::alignUp(buffer.getSize(), g_context.m_properties.limits.nonCoherentAtomSize);
	//
	//	vkInvalidateMappedMemoryRanges(g_context.m_device, 1, &range);
	//	m_opaqueDraws = *data;
	//	m_totalOpaqueDraws = m_totalOpaqueDrawsPending[commonData.m_curResIdx];
	//	m_totalOpaqueDrawsPending[commonData.m_curResIdx] = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount;
	//
	//	g_context.m_allocator.unmapMemory(buffer.getAllocation());
	//}

	m_sparseVoxelBricksModule->readBackAllocatedBrickCount(commonData.m_curResIdx);
	g_allocatedBricks = m_sparseVoxelBricksModule->getAllocatedBrickCount();
	m_diffuseGIProbesModule->readBackCulledProbesCount(commonData.m_curResIdx);
	m_opaqueDraws = m_diffuseGIProbesModule->getCulledProbeCount();

	// get timing data
	graph.getTimingInfo(&m_passTimingCount, &m_passTimingData);


	// import resources into graph
	m_sparseVoxelBricksModule->updateResourceHandles(graph);
	m_diffuseGIProbesModule->updateResourceHandles(graph);

	ImageViewHandle depthImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "Depth Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_depthImages[commonData.m_curResIdx].getFormat();

		ImageHandle imageHandle = graph.importImage(desc, m_renderResources->m_depthImages[commonData.m_curResIdx].getImage(), &m_renderResources->m_depthImageQueue[commonData.m_curResIdx], &m_renderResources->m_depthImageResourceState[commonData.m_curResIdx]);
		depthImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle prevDepthImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "Prev Depth Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_depthImages[commonData.m_prevResIdx].getFormat();

		ImageHandle imageHandle = graph.importImage(desc, m_renderResources->m_depthImages[commonData.m_prevResIdx].getImage(), &m_renderResources->m_depthImageQueue[commonData.m_prevResIdx], &m_renderResources->m_depthImageResourceState[commonData.m_prevResIdx]);
		prevDepthImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle lightImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "Light Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_levels = 1 + static_cast<uint32_t>(glm::floor(glm::log2(float(glm::max(desc.m_width, desc.m_height)))));
		desc.m_format = m_renderResources->m_lightImages[commonData.m_curResIdx].getFormat();

		ImageHandle imageHandle = graph.importImage(desc, m_renderResources->m_lightImages[commonData.m_curResIdx].getImage(), m_renderResources->m_lightImageQueue[commonData.m_curResIdx], m_renderResources->m_lightImageResourceState[commonData.m_curResIdx]);
		lightImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT , 0, 1, 0, 1 } });
	}

	ImageHandle prevLightImageHandle = 0;
	ImageViewHandle prevLightImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "Prev Light Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_levels = 1 + static_cast<uint32_t>(glm::floor(glm::log2(float(glm::max(desc.m_width, desc.m_height)))));
		desc.m_format = m_renderResources->m_lightImages[commonData.m_prevResIdx].getFormat();

		prevLightImageHandle = graph.importImage(desc, m_renderResources->m_lightImages[commonData.m_prevResIdx].getImage(), m_renderResources->m_lightImageQueue[commonData.m_prevResIdx], m_renderResources->m_lightImageResourceState[commonData.m_prevResIdx]);
		prevLightImageViewHandle = graph.createImageView({ desc.m_name, prevLightImageHandle, { VK_IMAGE_ASPECT_COLOR_BIT , 0, desc.m_levels, 0, 1 } });
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
		desc.m_format = m_renderResources->m_taaHistoryTextures[commonData.m_prevResIdx].getFormat();

		ImageHandle taaHistoryImageHandle = graph.importImage(desc, m_renderResources->m_taaHistoryTextures[commonData.m_prevResIdx].getImage(), &m_renderResources->m_taaHistoryTextureQueue[commonData.m_prevResIdx], &m_renderResources->m_taaHistoryTextureResourceState[commonData.m_prevResIdx]);
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
		desc.m_format = m_renderResources->m_taaHistoryTextures[commonData.m_curResIdx].getFormat();

		ImageHandle taaResolveImageHandle = graph.importImage(desc, m_renderResources->m_taaHistoryTextures[commonData.m_curResIdx].getImage(), &m_renderResources->m_taaHistoryTextureQueue[commonData.m_curResIdx], &m_renderResources->m_taaHistoryTextureResourceState[commonData.m_curResIdx]);
		taaResolveImageViewHandle = graph.createImageView({ desc.m_name, taaResolveImageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
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

	ImageViewHandle albedoImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_name = "Albedo Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R8G8B8A8_UNORM;

		albedoImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle normalImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_name = "Normals Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		normalImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageViewHandle ssrRayHitPdfImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_name = "SSR RayHit/PDF Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;

		ssrRayHitPdfImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}


	ImageViewHandle ssrMaskImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_name = "SSR Mask Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R8_UNORM;

		ssrMaskImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	ImageHandle deferredShadowsImageHandle;
	ImageViewHandle deferredShadowsImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_name = "Deferred Shadows Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_layers = static_cast<uint32_t>(lightData.m_directionalLightData.size());
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_R8_UNORM;

		deferredShadowsImageHandle = graph.createImage(desc);
		deferredShadowsImageViewHandle = graph.createImageView({ desc.m_name, deferredShadowsImageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, desc.m_layers }, VK_IMAGE_VIEW_TYPE_2D_ARRAY });
	}

	ImageViewHandle brdfLUTImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "BRDF LUT Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = 128;
		desc.m_height = 128;
		desc.m_format = m_renderResources->m_brdfLUT.getFormat();

		ImageHandle imageHandle = graph.importImage(desc, m_renderResources->m_brdfLUT.getImage(), &m_renderResources->m_brdfLutImageQueue, &m_renderResources->m_brdfLutImageResourceState);
		brdfLUTImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT , 0, 1, 0, 1 } });
	}

	ImageViewHandle indirectDiffuseImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	ImageViewHandle indirectSpecularImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	ImageViewHandle finalImageViewHandle = VKResourceDefinitions::createFinalImageViewHandle(graph, m_width, m_height);
	ImageViewHandle finalImageViewHandle2 = VKResourceDefinitions::createFinalImageViewHandle(graph, m_width, m_height);
	ImageViewHandle uvImageViewHandle = VKResourceDefinitions::createUVImageViewHandle(graph, m_width, m_height);
	ImageViewHandle ddxyLengthImageViewHandle = VKResourceDefinitions::createDerivativesLengthImageViewHandle(graph, m_width, m_height);
	ImageViewHandle ddxyRotMaterialIdImageViewHandle = VKResourceDefinitions::createDerivativesRotMaterialIdImageViewHandle(graph, m_width, m_height);
	ImageViewHandle tangentSpaceImageViewHandle = VKResourceDefinitions::createTangentSpaceImageViewHandle(graph, m_width, m_height);
	ImageViewHandle velocityImageViewHandle = VKResourceDefinitions::createVelocityImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle lightImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyAccumImageViewHandle = VKResourceDefinitions::createTransparencyAccumImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyTransmittanceImageViewHandle = VKResourceDefinitions::createTransparencyTransmittanceImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyDeltaImageViewHandle = VKResourceDefinitions::createTransparencyDeltaImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyResultImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle deferredShadowsImageViewHandle = VKResourceDefinitions::createDeferredShadowsImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle reprojectedDepthUintImageViewHandle = VKResourceDefinitions::createReprojectedDepthUintImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle reprojectedDepthImageViewHandle = VKResourceDefinitions::createReprojectedDepthImageViewHandle(graph, m_width, m_height);
	BufferViewHandle pointLightBitMaskBufferViewHandle = VKResourceDefinitions::createPointLightBitMaskBufferViewHandle(graph, m_width, m_height, static_cast<uint32_t>(lightData.m_pointLightData.size()));
	BufferViewHandle luminanceHistogramBufferViewHandle = VKResourceDefinitions::createLuminanceHistogramBufferViewHandle(graph);
	//BufferViewHandle indirectBufferViewHandle = VKResourceDefinitions::createIndirectBufferViewHandle(graph, renderData.m_subMeshInstanceDataCount);
	//BufferViewHandle visibilityBufferViewHandle = VKResourceDefinitions::createOcclusionCullingVisibilityBufferViewHandle(graph, renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount);
	//BufferViewHandle drawCountsBufferViewHandle = VKResourceDefinitions::createIndirectDrawCountsBufferViewHandle(graph, renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount);


	// transform data write
	VkDescriptorBufferInfo transformDataBufferInfo{ VK_NULL_HANDLE, 0, std::max(renderData.m_transformDataCount * sizeof(glm::mat4), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(transformDataBufferInfo.range, transformDataBufferInfo.offset, transformDataBufferInfo.buffer, bufferPtr);
		if (renderData.m_transformDataCount)
		{
			memcpy(bufferPtr, renderData.m_transformData, renderData.m_transformDataCount * sizeof(glm::mat4));
		}
	}

	// directional light data write
	VkDescriptorBufferInfo directionalLightDataBufferInfo{ VK_NULL_HANDLE, 0, std::max(lightData.m_directionalLightData.size() * sizeof(DirectionalLightData), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(directionalLightDataBufferInfo.range, directionalLightDataBufferInfo.offset, directionalLightDataBufferInfo.buffer, bufferPtr);
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
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(pointLightDataBufferInfo.range, pointLightDataBufferInfo.offset, pointLightDataBufferInfo.buffer, dataBufferPtr);
		uint8_t *zBinsBufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(pointLightZBinsBufferInfo.range, pointLightZBinsBufferInfo.offset, pointLightZBinsBufferInfo.buffer, zBinsBufferPtr);
		if (!lightData.m_pointLightData.empty())
		{
			memcpy(dataBufferPtr, lightData.m_pointLightData.data(), lightData.m_pointLightData.size() * sizeof(PointLightData));
			memcpy(zBinsBufferPtr, lightData.m_zBins.data(), lightData.m_zBins.size() * sizeof(uint32_t));
		}
	}

	// shadow matrices write
	VkDescriptorBufferInfo shadowMatricesBufferInfo{ VK_NULL_HANDLE, 0, std::max(renderData.m_shadowMatrixCount * sizeof(glm::mat4), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(shadowMatricesBufferInfo.range, shadowMatricesBufferInfo.offset, shadowMatricesBufferInfo.buffer, bufferPtr);
		if (renderData.m_shadowMatrixCount)
		{
			memcpy(bufferPtr, renderData.m_shadowMatrices, renderData.m_shadowMatrixCount * sizeof(glm::mat4));
		}
	}

	// shadow texel sized write
	VkDescriptorBufferInfo shadowCascadeParamsBufferInfo{ VK_NULL_HANDLE, 0, std::max(renderData.m_shadowMatrixCount * sizeof(glm::vec4), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(shadowCascadeParamsBufferInfo.range, shadowCascadeParamsBufferInfo.offset, shadowCascadeParamsBufferInfo.buffer, bufferPtr);
		if (renderData.m_shadowMatrixCount)
		{
			memcpy(bufferPtr, renderData.m_shadowCascadeParams, renderData.m_shadowMatrixCount * sizeof(glm::vec4));
		}
	}

	// instance data write
	std::vector<SubMeshInstanceData> sortedInstanceData(renderData.m_subMeshInstanceDataCount);
	VkDescriptorBufferInfo instanceDataBufferInfo{ VK_NULL_HANDLE, 0, std::max(renderData.m_subMeshInstanceDataCount * sizeof(SubMeshInstanceData), size_t(1)) };
	{
		uint8_t *bufferPtr;
		m_renderResources->m_mappableSSBOBlock[commonData.m_curResIdx]->allocate(instanceDataBufferInfo.range, instanceDataBufferInfo.offset, instanceDataBufferInfo.buffer, bufferPtr);

		// copy instance data to gpu memory and sort it according to the sorted drawCallKeys list
		SubMeshInstanceData *instanceData = (SubMeshInstanceData *)bufferPtr;
		for (size_t i = 0; i < renderData.m_subMeshInstanceDataCount; ++i)
		{
			instanceData[i] = sortedInstanceData[i] = renderData.m_subMeshInstanceData[static_cast<uint32_t>(renderData.m_drawCallKeys[i] & 0xFFFFFFFF)];
		}
	}

	PassRecordContext passRecordContext{};
	passRecordContext.m_renderResources = m_renderResources.get();
	passRecordContext.m_deferredObjectDeleter = m_deferredObjectDeleter.get();
	passRecordContext.m_renderPassCache = m_renderPassCache.get();
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

	//// reproject previous depth buffer to create occlusion culling depth buffer
	//OcclusionCullingReprojectionPass::Data occlusionCullingReprojData;
	//occlusionCullingReprojData.m_passRecordContext = &passRecordContext;
	//occlusionCullingReprojData.m_prevDepthImageViewHandle = prevDepthImageViewHandle;
	//occlusionCullingReprojData.m_resultImageViewHandle = reprojectedDepthUintImageViewHandle;

	//OcclusionCullingReprojectionPass::addToGraph(graph, occlusionCullingReprojData);


	//// copy from uint image to depth image
	//OcclusionCullingCopyToDepthPass::Data occlusionCullingCopyData;
	//occlusionCullingCopyData.m_passRecordContext = &passRecordContext;
	//occlusionCullingCopyData.m_inputImageHandle = reprojectedDepthUintImageViewHandle;
	//occlusionCullingCopyData.m_resultImageHandle = reprojectedDepthImageViewHandle;

	//OcclusionCullingCopyToDepthPass::addToGraph(graph, occlusionCullingCopyData);


	//// render obbs against reprojected depth buffer to test for occlusion
	//OcclusionCullingPass::Data occlusionCullingPassData;
	//occlusionCullingPassData.m_passRecordContext = &passRecordContext;
	//occlusionCullingPassData.m_drawOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueOffset;
	//occlusionCullingPassData.m_drawCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount;
	//occlusionCullingPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	//occlusionCullingPassData.m_transformDataBufferInfo = transformDataBufferInfo;
	//occlusionCullingPassData.m_aabbBufferInfo = { m_renderResources->m_subMeshBoundingBoxBuffer.getBuffer(), 0, m_renderResources->m_subMeshBoundingBoxBuffer.getSize() };
	//occlusionCullingPassData.m_visibilityBufferHandle = visibilityBufferViewHandle;
	//occlusionCullingPassData.m_depthImageHandle = reprojectedDepthImageViewHandle;
	//
	//OcclusionCullingPass::addToGraph(graph, occlusionCullingPassData);

	// Hi-Z furthest depth pyramid
	HiZPyramidPass::OutData hiZMinPyramidPassOutData;
	HiZPyramidPass::Data hiZMinPyramidPassData;
	hiZMinPyramidPassData.m_passRecordContext = &passRecordContext;
	hiZMinPyramidPassData.m_inputImageViewHandle = prevDepthImageViewHandle;
	hiZMinPyramidPassData.m_maxReduction = false;
	hiZMinPyramidPassData.m_copyFirstLevel = false;
	hiZMinPyramidPassData.m_forceExecution = true;

	HiZPyramidPass::addToGraph(graph, hiZMinPyramidPassData, hiZMinPyramidPassOutData);

	ImageViewHandle depthPyramidImageViewHandle = hiZMinPyramidPassOutData.m_resultImageViewHandle;

	//
	//// HiZ Culling
	//OcclusionCullingHiZPass::Data occlusionCullingHiZData;
	//occlusionCullingHiZData.m_passRecordContext = &passRecordContext;
	//occlusionCullingHiZData.m_drawOffset = occlusionCullingPassData.m_drawOffset;
	//occlusionCullingHiZData.m_drawCount = occlusionCullingPassData.m_drawCount;
	//occlusionCullingHiZData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	//occlusionCullingHiZData.m_transformDataBufferInfo = transformDataBufferInfo;
	//occlusionCullingHiZData.m_aabbBufferInfo = { m_renderResources->m_subMeshBoundingBoxBuffer.getBuffer(), 0, m_renderResources->m_subMeshBoundingBoxBuffer.getSize() };
	//occlusionCullingHiZData.m_visibilityBufferHandle = visibilityBufferViewHandle;
	//occlusionCullingHiZData.m_depthPyramidImageHandle = depthPyramidImageViewHandle;
	//
	////OcclusionCullingHiZPass::addToGraph(graph, occlusionCullingHiZData);
	//
	//
	//// compact occlusion culled draws
	//OcclusionCullingCreateDrawArgsPass::Data occlusionCullingDrawArgsData;
	//occlusionCullingDrawArgsData.m_passRecordContext = &passRecordContext;
	//occlusionCullingDrawArgsData.m_drawOffset = occlusionCullingPassData.m_drawOffset;
	//occlusionCullingDrawArgsData.m_drawCount = occlusionCullingPassData.m_drawCount;
	//occlusionCullingDrawArgsData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	//occlusionCullingDrawArgsData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
	//occlusionCullingDrawArgsData.m_indirectBufferHandle = indirectBufferViewHandle;
	//occlusionCullingDrawArgsData.m_drawCountsBufferHandle = drawCountsBufferViewHandle;
	//occlusionCullingDrawArgsData.m_visibilityBufferHandle = visibilityBufferViewHandle;
	//
	////OcclusionCullingCreateDrawArgsPass::addToGraph(graph, occlusionCullingDrawArgsData);
	//
	//// copy draw count to readback buffer
	//ReadBackCopyPass::Data drawCountReadBackCopyPassData;
	//drawCountReadBackCopyPassData.m_passRecordContext = &passRecordContext;
	//drawCountReadBackCopyPassData.m_bufferCopy = { 0, 0, sizeof(uint32_t) };
	//drawCountReadBackCopyPassData.m_srcBuffer = drawCountsBufferViewHandle;
	//drawCountReadBackCopyPassData.m_dstBuffer = m_renderResources->m_occlusionCullStatsReadBackBuffers[commonData.m_curResIdx].getBuffer();
	//
	//ReadBackCopyPass::addToGraph(graph, drawCountReadBackCopyPassData);

	BufferViewHandle opaqueIndirectDrawBufferViewHandle = nullptr;
	BufferViewHandle opaqueFilteredIndicesBufferViewHandle = nullptr;

	// opaque geometry
	if (renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount)
	{
		// build index buffer
		BuildIndexBufferPass::Data buildIndexBufferPassData;
		buildIndexBufferPassData.m_passRecordContext = &passRecordContext;
		buildIndexBufferPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
		buildIndexBufferPassData.m_instanceData = sortedInstanceData.data();
		buildIndexBufferPassData.m_async = false;
		buildIndexBufferPassData.m_cullBackFace = true;
		buildIndexBufferPassData.m_viewProjectionMatrix = commonData.m_jitteredViewProjectionMatrix;
		buildIndexBufferPassData.m_width = m_width;
		buildIndexBufferPassData.m_height = m_height;
		buildIndexBufferPassData.m_instanceOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueOffset;
		buildIndexBufferPassData.m_instanceCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount;
		buildIndexBufferPassData.m_transformDataBufferInfo = transformDataBufferInfo;
		buildIndexBufferPassData.m_indirectDrawCmdBufferViewHandle = &opaqueIndirectDrawBufferViewHandle;
		buildIndexBufferPassData.m_filteredIndicesBufferViewHandle = &opaqueFilteredIndicesBufferViewHandle;

		BuildIndexBufferPass::addToGraph(graph, buildIndexBufferPassData);


		// draw opaque geometry to gbuffer
		VKGeometryPass::Data opaqueGeometryPassData;
		opaqueGeometryPassData.m_passRecordContext = &passRecordContext;
		opaqueGeometryPassData.m_alphaMasked = false;
		opaqueGeometryPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
		opaqueGeometryPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
		opaqueGeometryPassData.m_transformDataBufferInfo = transformDataBufferInfo;
		opaqueGeometryPassData.m_indirectBufferHandle = opaqueIndirectDrawBufferViewHandle;
		opaqueGeometryPassData.m_depthImageHandle = depthImageViewHandle;
		opaqueGeometryPassData.m_uvImageHandle = uvImageViewHandle;
		opaqueGeometryPassData.m_ddxyLengthImageHandle = ddxyLengthImageViewHandle;
		opaqueGeometryPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageViewHandle;
		opaqueGeometryPassData.m_tangentSpaceImageHandle = tangentSpaceImageViewHandle;
		opaqueGeometryPassData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
		opaqueGeometryPassData.m_indicesBufferHandle = opaqueFilteredIndicesBufferViewHandle;

		VKGeometryPass::addToGraph(graph, opaqueGeometryPassData);
	}

	// alpha masked geometry
	if (renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedCount)
	{
		// build index buffer
		BufferViewHandle indirectDrawBufferViewHandle;
		BufferViewHandle filteredIndicesBufferViewHandle;

		BuildIndexBufferPass::Data buildIndexBufferPassData;
		buildIndexBufferPassData.m_passRecordContext = &passRecordContext;
		buildIndexBufferPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
		buildIndexBufferPassData.m_instanceData = sortedInstanceData.data();
		buildIndexBufferPassData.m_async = false;
		buildIndexBufferPassData.m_cullBackFace = false;
		buildIndexBufferPassData.m_viewProjectionMatrix = commonData.m_jitteredViewProjectionMatrix;
		buildIndexBufferPassData.m_width = m_width;
		buildIndexBufferPassData.m_height = m_height;
		buildIndexBufferPassData.m_instanceOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedOffset;
		buildIndexBufferPassData.m_instanceCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedCount;
		buildIndexBufferPassData.m_transformDataBufferInfo = transformDataBufferInfo;
		buildIndexBufferPassData.m_indirectDrawCmdBufferViewHandle = &indirectDrawBufferViewHandle;
		buildIndexBufferPassData.m_filteredIndicesBufferViewHandle = &filteredIndicesBufferViewHandle;

		BuildIndexBufferPass::addToGraph(graph, buildIndexBufferPassData);


		// draw alpha masked geometry to gbuffer
		VKGeometryPass::Data maskedGeometryPassData;
		maskedGeometryPassData.m_passRecordContext = &passRecordContext;
		maskedGeometryPassData.m_alphaMasked = true;
		maskedGeometryPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
		maskedGeometryPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
		maskedGeometryPassData.m_transformDataBufferInfo = transformDataBufferInfo;
		maskedGeometryPassData.m_indirectBufferHandle = indirectDrawBufferViewHandle;
		maskedGeometryPassData.m_depthImageHandle = depthImageViewHandle;
		maskedGeometryPassData.m_uvImageHandle = uvImageViewHandle;
		maskedGeometryPassData.m_ddxyLengthImageHandle = ddxyLengthImageViewHandle;
		maskedGeometryPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageViewHandle;
		maskedGeometryPassData.m_tangentSpaceImageHandle = tangentSpaceImageViewHandle;
		maskedGeometryPassData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
		maskedGeometryPassData.m_indicesBufferHandle = filteredIndicesBufferViewHandle;

		VKGeometryPass::addToGraph(graph, maskedGeometryPassData);
	}

	// initialize velocity of static objects
	VKVelocityInitializationPass::Data velocityInitializationPassData;
	velocityInitializationPassData.m_passRecordContext = &passRecordContext;
	velocityInitializationPassData.m_depthImageHandle = depthImageViewHandle;
	velocityInitializationPassData.m_velocityImageHandle = velocityImageViewHandle;

	VKVelocityInitializationPass::addToGraph(graph, velocityInitializationPassData);

	ImageViewHandle shadowImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "Shadow Cascades Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = 2048;
		desc.m_height = 2048;
		desc.m_layers = glm::max(renderData.m_shadowCascadeViewRenderListCount, 1u);
		desc.m_levels = 1;
		desc.m_samples = 1;
		desc.m_format = VK_FORMAT_D16_UNORM;

		ImageHandle shadowImageHandle = graph.createImage(desc);
		shadowImageViewHandle = graph.createImageView({ desc.m_name, shadowImageHandle, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, desc.m_layers }, VK_IMAGE_VIEW_TYPE_2D_ARRAY });

		for (uint32_t i = 0; i < renderData.m_shadowCascadeViewRenderListCount; ++i)
		{
			ImageViewHandle shadowLayer = graph.createImageView({ desc.m_name, shadowImageHandle, { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, i, 1 } });

			const auto &drawList = renderData.m_renderLists[renderData.m_shadowCascadeViewRenderListOffset + i];

			// draw shadows
			if (drawList.m_opaqueCount)
			{
				// build index buffer
				BufferViewHandle indirectDrawBufferViewHandle;
				BufferViewHandle filteredIndicesBufferViewHandle;

				BuildIndexBufferPass::Data buildIndexBufferPassData;
				buildIndexBufferPassData.m_passRecordContext = &passRecordContext;
				buildIndexBufferPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
				buildIndexBufferPassData.m_instanceData = sortedInstanceData.data();
				buildIndexBufferPassData.m_async = true;
				buildIndexBufferPassData.m_cullBackFace = true;
				buildIndexBufferPassData.m_viewProjectionMatrix = renderData.m_shadowMatrices[i];
				buildIndexBufferPassData.m_width = 2048;
				buildIndexBufferPassData.m_height = 2048;
				buildIndexBufferPassData.m_instanceOffset = drawList.m_opaqueOffset;
				buildIndexBufferPassData.m_instanceCount = drawList.m_opaqueCount;
				buildIndexBufferPassData.m_transformDataBufferInfo = transformDataBufferInfo;
				buildIndexBufferPassData.m_indirectDrawCmdBufferViewHandle = &indirectDrawBufferViewHandle;
				buildIndexBufferPassData.m_filteredIndicesBufferViewHandle = &filteredIndicesBufferViewHandle;

				BuildIndexBufferPass::addToGraph(graph, buildIndexBufferPassData);

				VKShadowPass::Data opaqueShadowPassData;
				opaqueShadowPassData.m_passRecordContext = &passRecordContext;
				opaqueShadowPassData.m_shadowMapSize = 2048;
				opaqueShadowPassData.m_shadowMatrix = renderData.m_shadowMatrices[i];
				opaqueShadowPassData.m_alphaMasked = false;
				opaqueShadowPassData.m_clear = true;
				opaqueShadowPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
				opaqueShadowPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
				opaqueShadowPassData.m_transformDataBufferInfo = transformDataBufferInfo;
				opaqueShadowPassData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
				opaqueShadowPassData.m_indicesBufferHandle = filteredIndicesBufferViewHandle;
				opaqueShadowPassData.m_indirectBufferHandle = indirectDrawBufferViewHandle;
				opaqueShadowPassData.m_shadowImageHandle = shadowLayer;

				VKShadowPass::addToGraph(graph, opaqueShadowPassData);
			}



			// draw masked shadows
			if (drawList.m_maskedCount)
			{
				// build index buffer
				BufferViewHandle indirectDrawBufferViewHandle;
				BufferViewHandle filteredIndicesBufferViewHandle;

				BuildIndexBufferPass::Data buildIndexBufferPassData;
				buildIndexBufferPassData.m_passRecordContext = &passRecordContext;
				buildIndexBufferPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
				buildIndexBufferPassData.m_instanceData = sortedInstanceData.data();
				buildIndexBufferPassData.m_async = true;
				buildIndexBufferPassData.m_cullBackFace = false;
				buildIndexBufferPassData.m_viewProjectionMatrix = renderData.m_shadowMatrices[i];
				buildIndexBufferPassData.m_width = 2048;
				buildIndexBufferPassData.m_height = 2048;
				buildIndexBufferPassData.m_instanceOffset = drawList.m_maskedOffset;
				buildIndexBufferPassData.m_instanceCount = drawList.m_maskedCount;
				buildIndexBufferPassData.m_transformDataBufferInfo = transformDataBufferInfo;
				buildIndexBufferPassData.m_indirectDrawCmdBufferViewHandle = &indirectDrawBufferViewHandle;
				buildIndexBufferPassData.m_filteredIndicesBufferViewHandle = &filteredIndicesBufferViewHandle;

				BuildIndexBufferPass::addToGraph(graph, buildIndexBufferPassData);

				VKShadowPass::Data maskedShadowPassData;
				maskedShadowPassData.m_passRecordContext = &passRecordContext;
				maskedShadowPassData.m_shadowMapSize = 2048;
				maskedShadowPassData.m_shadowMatrix = renderData.m_shadowMatrices[i];
				maskedShadowPassData.m_alphaMasked = true;
				maskedShadowPassData.m_clear = drawList.m_opaqueCount == 0;
				maskedShadowPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
				maskedShadowPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
				maskedShadowPassData.m_transformDataBufferInfo = transformDataBufferInfo;
				maskedShadowPassData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
				maskedShadowPassData.m_indicesBufferHandle = filteredIndicesBufferViewHandle;
				maskedShadowPassData.m_indirectBufferHandle = indirectDrawBufferViewHandle;
				maskedShadowPassData.m_shadowImageHandle = shadowLayer;

				VKShadowPass::addToGraph(graph, maskedShadowPassData);
			}
		}
	}


	// deferred shadows
	DeferredShadowsPass::Data deferredShadowsPassData;
	deferredShadowsPassData.m_passRecordContext = &passRecordContext;
	deferredShadowsPassData.m_lightDataCount = static_cast<uint32_t>(lightData.m_directionalLightData.size());
	deferredShadowsPassData.m_lightData = lightData.m_directionalLightData.data();
	deferredShadowsPassData.m_resultImageHandle = deferredShadowsImageHandle;
	deferredShadowsPassData.m_depthImageViewHandle = depthImageViewHandle;
	deferredShadowsPassData.m_tangentSpaceImageViewHandle = tangentSpaceImageViewHandle;
	deferredShadowsPassData.m_shadowImageViewHandle = shadowImageViewHandle;
	deferredShadowsPassData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;
	deferredShadowsPassData.m_cascadeParamsBufferInfo = shadowCascadeParamsBufferInfo;

	DeferredShadowsPass::addToGraph(graph, deferredShadowsPassData);

	// gtao
	GTAOModule::Data gtaoModuleData;
	gtaoModuleData.m_passRecordContext = &passRecordContext;
	gtaoModuleData.m_depthImageViewHandle = depthImageViewHandle;
	gtaoModuleData.m_tangentSpaceImageViewHandle = tangentSpaceImageViewHandle;
	gtaoModuleData.m_velocityImageViewHandle = velocityImageViewHandle;

	if (g_ssaoEnabled)
	{
		m_gtaoModule->addToGraph(graph, gtaoModuleData);
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
	LightingPass::Data lightingPassData;
	lightingPassData.m_passRecordContext = &passRecordContext;
	lightingPassData.m_directionalLightDataBufferInfo = directionalLightDataBufferInfo;
	lightingPassData.m_pointLightDataBufferInfo = pointLightDataBufferInfo;
	lightingPassData.m_pointLightZBinsBufferInfo = pointLightZBinsBufferInfo;
	lightingPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
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

	LightingPass::addToGraph(graph, lightingPassData);


	// indirect diffuse
	IndirectDiffusePass::Data indirectDiffusePassData;
	indirectDiffusePassData.m_passRecordContext = &passRecordContext;
	indirectDiffusePassData.m_ssao = g_ssaoEnabled;
	indirectDiffusePassData.m_irradianceVolumeImageHandle = m_diffuseGIProbesModule->getIrradianceVolumeImageViewHandle();
	indirectDiffusePassData.m_irradianceVolumeDepthImageHandle = m_diffuseGIProbesModule->getIrradianceVolumeDepthImageViewHandle();
	indirectDiffusePassData.m_depthImageHandle = depthImageViewHandle;
	//indirectDiffusePassData.m_albedoImageHandle = albedoImageViewHandle;
	indirectDiffusePassData.m_normalImageHandle = normalImageViewHandle;
	indirectDiffusePassData.m_occlusionImageHandle = m_gtaoModule->getAOResultImageViewHandle();
	indirectDiffusePassData.m_resultImageHandle = indirectDiffuseImageViewHandle;

	IndirectDiffusePass::addToGraph(graph, indirectDiffusePassData);


	// generate mip pyramid of previous lighting image
	GenerateMipMapsPass::Data generateMipMapsPassData;
	generateMipMapsPassData.m_passRecordContext = &passRecordContext;
	generateMipMapsPassData.m_width = m_width;
	generateMipMapsPassData.m_height = m_height;
	generateMipMapsPassData.m_mipCount = 1 + static_cast<uint32_t>(glm::floor(glm::log2(float(glm::max(m_width, m_height)))));
	generateMipMapsPassData.m_imageHandle = prevLightImageHandle;

	GenerateMipMapsPass::addToGraph(graph, generateMipMapsPassData);


	//// Hi-Z closest depth pyramid
	//HiZPyramidPass::OutData hiZMaxPyramidPassOutData;
	//HiZPyramidPass::Data hiZMaxPyramidPassData;
	//hiZMaxPyramidPassData.m_passRecordContext = &passRecordContext;
	//hiZMaxPyramidPassData.m_inputImageViewHandle = depthImageViewHandle;
	//hiZMaxPyramidPassData.m_maxReduction = true;
	//hiZMaxPyramidPassData.m_copyFirstLevel = true;
	//hiZMaxPyramidPassData.m_forceExecution = false;
	//
	//HiZPyramidPass::addToGraph(graph, hiZMaxPyramidPassData, hiZMaxPyramidPassOutData);
	//
	//
	//// screen space reflections
	//SSRModule::Data ssrModuleData;
	//ssrModuleData.m_passRecordContext = &passRecordContext;
	//ssrModuleData.m_noiseTextureHandle = m_blueNoiseTextureIndex;
	//ssrModuleData.m_hiZPyramidImageViewHandle = hiZMaxPyramidPassOutData.m_resultImageViewHandle;
	//ssrModuleData.m_normalImageViewHandle = normalImageViewHandle;
	//ssrModuleData.m_depthImageViewHandle = depthImageViewHandle;
	//ssrModuleData.m_albedoImageViewHandle = albedoImageViewHandle;
	//ssrModuleData.m_prevColorImageViewHandle = prevLightImageViewHandle;
	//ssrModuleData.m_velocityImageViewHandle = velocityImageViewHandle;
	//
	//m_ssrModule->addToGraph(graph, ssrModuleData);


	// apply indirect light
	IndirectLightPass::Data indirectLightPassData;
	indirectLightPassData.m_passRecordContext = &passRecordContext;
	indirectLightPassData.m_depthImageHandle = depthImageViewHandle;
	indirectLightPassData.m_albedoImageHandle = albedoImageViewHandle;
	indirectLightPassData.m_normalImageHandle = normalImageViewHandle;
	indirectLightPassData.m_indirectDiffuseImageHandle = indirectDiffuseImageViewHandle;
	indirectLightPassData.m_indirectSpecularImageHandle = indirectSpecularImageViewHandle;
	indirectLightPassData.m_brdfImageHandle = brdfLUTImageViewHandle;
	indirectLightPassData.m_resultImageHandle = lightImageViewHandle;

	IndirectLightPass::addToGraph(graph, indirectLightPassData);


	// update voxel representation of scene
	SparseVoxelBricksModule::Data sparseVoxelBricksModuleData;
	sparseVoxelBricksModuleData.m_passRecordContext = &passRecordContext;
	sparseVoxelBricksModuleData.m_instanceDataCount = renderData.m_allInstanceDataCount;
	sparseVoxelBricksModuleData.m_instanceData = renderData.m_allInstanceData;
	sparseVoxelBricksModuleData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
	sparseVoxelBricksModuleData.m_transformDataBufferInfo = transformDataBufferInfo;
	sparseVoxelBricksModuleData.m_directionalLightDataBufferInfo = directionalLightDataBufferInfo;
	sparseVoxelBricksModuleData.m_pointLightDataBufferInfo = pointLightDataBufferInfo;
	sparseVoxelBricksModuleData.m_pointLightZBinsBufferInfo = pointLightZBinsBufferInfo;
	sparseVoxelBricksModuleData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;
	sparseVoxelBricksModuleData.m_irradianceVolumeImageViewHandle = m_diffuseGIProbesModule->getIrradianceVolumeImageViewHandle();
	sparseVoxelBricksModuleData.m_irradianceVolumeDepthImageViewHandle = m_diffuseGIProbesModule->getIrradianceVolumeDepthImageViewHandle();
	sparseVoxelBricksModuleData.m_depthImageViewHandle = depthImageViewHandle;
	sparseVoxelBricksModuleData.m_uvImageViewHandle = uvImageViewHandle;
	sparseVoxelBricksModuleData.m_ddxyLengthImageViewHandle = ddxyLengthImageViewHandle;
	sparseVoxelBricksModuleData.m_ddxyRotMaterialIdImageViewHandle = ddxyRotMaterialIdImageViewHandle;
	sparseVoxelBricksModuleData.m_tangentSpaceImageViewHandle = tangentSpaceImageViewHandle;
	sparseVoxelBricksModuleData.m_deferredShadowsImageViewHandle = deferredShadowsImageViewHandle;
	sparseVoxelBricksModuleData.m_shadowImageViewHandle = shadowImageViewHandle;
	sparseVoxelBricksModuleData.m_pointLightBitMaskBufferViewHandle = pointLightBitMaskBufferViewHandle;

	m_sparseVoxelBricksModule->addVoxelizationToGraph(graph, sparseVoxelBricksModuleData);


	// voxel debug visualization
	if (g_giVoxelDebugMode == 6)
	{
		m_sparseVoxelBricksModule->addDebugVisualizationToGraph(graph, { &passRecordContext, lightImageViewHandle });
	}


	// update diffuse GI probes
	DiffuseGIProbesModule::Data diffuseGIProbesModuleData;
	diffuseGIProbesModuleData.m_passRecordContext = &passRecordContext;
	diffuseGIProbesModuleData.m_depthPyramidImageViewHandle = depthPyramidImageViewHandle;
	diffuseGIProbesModuleData.m_brickPointerImageViewHandle = m_sparseVoxelBricksModule->getBrickPointerImageViewHandle();
	diffuseGIProbesModuleData.m_binVisBricksImageViewHandle = m_sparseVoxelBricksModule->getBinVisImageViewHandle();
	diffuseGIProbesModuleData.m_colorBricksImageViewHandle = m_sparseVoxelBricksModule->getColorImageViewHandle();

	m_diffuseGIProbesModule->addProbeUpdateToGraph(graph, diffuseGIProbesModuleData);


	// irradiance volume debug visualization
	if (g_giVoxelDebugMode == 3 || g_giVoxelDebugMode == 4)
	{
		DiffuseGIProbesModule::DebugVisualizationData data;
		data.m_passRecordContext = &passRecordContext;
		data.m_cascadeIndex = g_debugVoxelCascadeIndex;
		data.m_showAge = g_giVoxelDebugMode == 4;
		data.m_colorImageViewHandle = lightImageViewHandle;
		data.m_depthImageViewHandle = depthImageViewHandle;

		m_diffuseGIProbesModule->addDebugVisualizationToGraph(graph, data);
	}


	// raytrace test
	RayTraceTestPass::Data rayTraceTestPassData;
	rayTraceTestPassData.m_passRecordContext = &passRecordContext;
	rayTraceTestPassData.m_resultImageHandle = lightImageViewHandle;
	rayTraceTestPassData.m_internalNodesBufferInfo = { m_renderResources->m_bvhNodesBuffer.getBuffer(), 0, m_renderResources->m_bvhNodesBuffer.getSize() };
	rayTraceTestPassData.m_leafNodesBufferInfo = { m_renderResources->m_bvhTrianglesBuffer.getBuffer(), 0, m_renderResources->m_bvhTrianglesBuffer.getSize() };

	//RayTraceTestPass::addToGraph(graph, rayTraceTestPassData);


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

	// copy luminance histogram to readback buffer
	ReadBackCopyPass::Data luminanceHistogramReadBackCopyPassData;
	luminanceHistogramReadBackCopyPassData.m_passRecordContext = &passRecordContext;
	luminanceHistogramReadBackCopyPassData.m_bufferCopy = { 0, 0, RendererConsts::LUMINANCE_HISTOGRAM_SIZE * sizeof(uint32_t) };
	luminanceHistogramReadBackCopyPassData.m_srcBuffer = luminanceHistogramBufferViewHandle;
	luminanceHistogramReadBackCopyPassData.m_dstBuffer = m_renderResources->m_luminanceHistogramReadBackBuffers[commonData.m_curResIdx].getBuffer();

	ReadBackCopyPass::addToGraph(graph, luminanceHistogramReadBackCopyPassData);


	// read back allocated brick count to host buffer
	m_sparseVoxelBricksModule->addAllocatedBrickCountReadBackCopyToGraph(graph, &passRecordContext);


	// get swapchain image
	VkSemaphore swapChainAvailableSemaphore = VK_NULL_HANDLE;
	ImageViewHandle swapchainImageViewHandle = 0;
	{
		m_swapChain->getCurrentImageIndex(commonData.m_frame, m_swapChainImageIndex, swapChainAvailableSemaphore);
		
		ImageDescription desc{};
		desc.m_name = "Swapchain Image";
		desc.m_concurrent = false;
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_swapChain->getImageFormat();

		auto imageHandle = graph.importImage(desc, m_swapChain->getImage(m_swapChainImageIndex));
		swapchainImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}


	// taa resolve
	//VKTAAResolvePass::Data taaResolvePassData;
	//taaResolvePassData.m_passRecordContext = &passRecordContext;
	//taaResolvePassData.m_jitterOffsetX = commonData.m_jitteredProjectionMatrix[2][0];
	//taaResolvePassData.m_jitterOffsetY = commonData.m_jitteredProjectionMatrix[2][1];
	//taaResolvePassData.m_depthImageHandle = depthImageViewHandle;
	//taaResolvePassData.m_velocityImageHandle = velocityImageViewHandle;
	//taaResolvePassData.m_taaHistoryImageHandle = taaHistoryImageViewHandle;
	//taaResolvePassData.m_lightImageHandle = lightImageViewHandle;
	//taaResolvePassData.m_taaResolveImageHandle = taaResolveImageViewHandle;
	//
	//if (g_TAAEnabled)
	//{
	//	VKTAAResolvePass::addToGraph(graph, taaResolvePassData);
	//
	//	lightImageViewHandle = taaResolveImageViewHandle;
	//}


	TAAPass::Data taaPassData;
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
		TAAPass::addToGraph(graph, taaPassData);

		lightImageViewHandle = taaResolveImageViewHandle;
	}


	ImageViewHandle currentOutput = lightImageViewHandle;


	// bloom
	ImageViewHandle bloomImageViewHandle = 0;
	BloomModule::OutputData bloomModuleOutData;
	BloomModule::InputData bloomModuleInData;
	bloomModuleInData.m_passRecordContext = &passRecordContext;
	bloomModuleInData.m_colorImageViewHandle = currentOutput;

	if (g_bloomEnabled)
	{
		BloomModule::addToGraph(graph, bloomModuleInData, bloomModuleOutData);
		bloomImageViewHandle = bloomModuleOutData.m_bloomImageViewHandle;
	}


	ImageViewHandle tonemapTargetTextureHandle = g_FXAAEnabled || g_CASEnabled ? finalImageViewHandle : swapchainImageViewHandle;

	// tonemap
	VKTonemapPass::Data tonemapPassData;
	tonemapPassData.m_passRecordContext = &passRecordContext;
	tonemapPassData.m_bloomEnabled = g_bloomEnabled;
	tonemapPassData.m_bloomStrength = g_bloomStrength;
	tonemapPassData.m_applyLinearToGamma = g_FXAAEnabled || !g_CASEnabled;
	tonemapPassData.m_srcImageHandle = currentOutput;
	tonemapPassData.m_dstImageHandle = tonemapTargetTextureHandle;
	tonemapPassData.m_bloomImageViewHandle = bloomImageViewHandle;
	tonemapPassData.m_avgLuminanceBufferHandle = avgLuminanceBufferViewHandle;

	VKTonemapPass::addToGraph(graph, tonemapPassData);

	currentOutput = tonemapTargetTextureHandle;


	ImageViewHandle fxaaTargetTextureHandle = g_CASEnabled ? finalImageViewHandle2 : swapchainImageViewHandle;

	// FXAA
	VKFXAAPass::Data fxaaPassData;
	fxaaPassData.m_passRecordContext = &passRecordContext;
	fxaaPassData.m_inputImageHandle = currentOutput;
	fxaaPassData.m_resultImageHandle = fxaaTargetTextureHandle;

	if (g_FXAAEnabled)
	{
		VKFXAAPass::addToGraph(graph, fxaaPassData);
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



	//// mesh cluster visualization
	//MeshClusterVisualizationPass::Data clusterVizPassData;
	//clusterVizPassData.m_passRecordContext = &passRecordContext;
	//clusterVizPassData.m_drawOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueOffset;
	//clusterVizPassData.m_drawCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount;
	//clusterVizPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	//clusterVizPassData.m_transformDataBufferInfo = transformDataBufferInfo;
	//clusterVizPassData.m_indirectBufferHandle = indirectBufferViewHandle;
	//clusterVizPassData.m_depthImageHandle = depthImageViewHandle;
	//clusterVizPassData.m_colorImageHandle = swapchainImageViewHandle;

	//MeshClusterVisualizationPass::addToGraph(graph, clusterVizPassData);
	//
	//clusterVizPassData.m_drawOffset = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedOffset;
	//clusterVizPassData.m_drawCount = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_maskedCount;
	//
	//MeshClusterVisualizationPass::addToGraph(graph, clusterVizPassData);


	// ImGui
	ImGuiPass::Data imGuiPassData;
	imGuiPassData.m_passRecordContext = &passRecordContext;
	imGuiPassData.m_imGuiDrawData = ImGui::GetDrawData();
	imGuiPassData.m_resultImageViewHandle = swapchainImageViewHandle;

	ImGuiPass::addToGraph(graph, imGuiPassData);

	uint32_t semaphoreCount;
	VkSemaphore *semaphores;
	graph.execute(ResourceViewHandle(swapchainImageViewHandle), swapChainAvailableSemaphore, &semaphoreCount, &semaphores, ResourceState::PRESENT_IMAGE, QueueType::GRAPHICS);

	m_swapChain->present(semaphoreCount, semaphores);
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

const uint32_t *VEngine::VKRenderer::getLuminanceHistogram() const
{
	return m_luminanceHistogram;
}

std::vector<VEngine::VKMemoryBlockDebugInfo> VEngine::VKRenderer::getMemoryAllocatorDebugInfo() const
{
	return g_context.m_allocator.getDebugInfo();
}

void VEngine::VKRenderer::getTimingInfo(size_t *count, const PassTimingInfo **data) const
{
	*count = m_passTimingCount;
	*data = m_passTimingData;
}

void VEngine::VKRenderer::getOcclusionCullingStats(uint32_t &draws, uint32_t &totalDraws) const
{
	draws = m_opaqueDraws;
	totalDraws = m_totalOpaqueDraws;
}

void VEngine::VKRenderer::setBVH(uint32_t nodeCount, const BVHNode *nodes, uint32_t triangleCount, const Triangle *triangles)
{
	m_renderResources->setBVH(nodeCount, nodes, triangleCount, triangles);
}

void VEngine::VKRenderer::resize(uint32_t width, uint32_t height)
{
	vkDeviceWaitIdle(g_context.m_device);
	m_width = width;
	m_height = height;
	if (m_width > 0 && m_height > 0)
	{
		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			m_frameGraphs[i]->reset();
		}
		m_swapChain->recreate(width, height);
		m_renderResources->resize(width, height);
		m_gtaoModule->resize(width, height);
		m_ssrModule->resize(width, height);
	}
}