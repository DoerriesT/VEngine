#include "VKRenderer.h"
#include "VKSwapChain.h"
#include "VKRenderResources.h"
#include "Utility/Utility.h"
#include "VKUtility.h"
#include "Graphics/RenderData.h"
#include "Graphics/LightData.h"
#include "VKTextureLoader.h"
#include "GlobalVar.h"
#include "Pass/VKGeometryPass.h"
#include "Pass/VKShadowPass.h"
#include "Pass/VKRasterTilingPass.h"
#include "Pass/VKLuminanceHistogramPass.h"
#include "Pass/VKLuminanceHistogramReduceAveragePass.h"
#include "Pass/VKTonemapPass.h"
#include "Pass/VKTAAResolvePass.h"
#include "Pass/VKVelocityInitializationPass.h"
#include "Pass/VKFXAAPass.h"
#include "Pass/VKGTAOPass.h"
#include "Pass/VKGTAOSpatialFilterPass.h"
#include "Pass/VKGTAOTemporalFilterPass.h"
#include "Pass/VKDirectLightingPass.h"
#include "Pass/DeferredShadowsPass.h"
#include "Pass/ImGuiPass.h"
#include "Pass/ReadBackCopyPass.h"
#include "Pass/MeshClusterVisualizationPass.h"
#include "Pass/OcclusionCullingReprojectionPass.h"
#include "Pass/OcclusionCullingCopyToDepthPass.h"
#include "Pass/OcclusionCullingPass.h"
#include "Pass/OcclusionCullingCreateDrawArgsPass.h"
#include "Pass/OcclusionCullingHiZPass.h"
#include "Pass/DepthPyramidPass.h"
#include "Pass/BuildIndexBufferPass.h"
#include "Pass/ScreenSpaceVoxelizationPass.h"
#include "Pass/ClearVoxelsPass.h"
#include "Pass/VoxelDebugPass.h"
#include "Pass/VoxelDebug2Pass.h"
#include "Pass/IrradianceVolumeDebugPass.h"
#include "Pass/UpdateQueueProbabilityPass.h"
#include "Pass/FillLightingQueuesPass.h"
#include "Pass/LightIrradianceVolumePass.h"
#include "Pass/VoxelizationPass.h"
#include "Pass/IrradianceVolumeRayMarchingPass.h"
#include "Pass/IrradianceVolumeUpdateProbesPass.h"
#include "Pass/IrradianceVolumeUpdateACProbesPass.h"
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
	{
		auto &buffer = m_renderResources->m_occlusionCullStatsReadBackBuffers[commonData.m_curResIdx];
		uint32_t *data;
		g_context.m_allocator.mapMemory(buffer.getAllocation(), (void **)&data);
	
		VkMappedMemoryRange range{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		range.memory = buffer.getDeviceMemory();
		range.offset = Utility::alignDown(buffer.getOffset(), g_context.m_properties.limits.nonCoherentAtomSize);
		range.size = Utility::alignUp(buffer.getSize(), g_context.m_properties.limits.nonCoherentAtomSize);
	
		vkInvalidateMappedMemoryRanges(g_context.m_device, 1, &range);
		m_opaqueDraws = *data;
		m_totalOpaqueDraws = m_totalOpaqueDrawsPending[commonData.m_curResIdx];
		m_totalOpaqueDrawsPending[commonData.m_curResIdx] = renderData.m_renderLists[renderData.m_mainViewRenderListIndex].m_opaqueCount;
	
		g_context.m_allocator.unmapMemory(buffer.getAllocation());
	}

	// get timing data
	graph.getTimingInfo(&m_passTimingCount, &m_passTimingData);

	// import resources into graph

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

	ImageViewHandle gtaoPreviousImageViewHandle = 0;
	{
		ImageDescription desc = {};
		desc.m_name = "GTAO Previous Texture";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = m_width;
		desc.m_height = m_height;
		desc.m_format = m_renderResources->m_gtaoHistoryTextures[commonData.m_prevResIdx].getFormat();

		ImageHandle gtaoPreviousImageHandle = graph.importImage(desc, m_renderResources->m_gtaoHistoryTextures[commonData.m_prevResIdx].getImage(), &m_renderResources->m_gtaoHistoryTextureQueue[commonData.m_prevResIdx], &m_renderResources->m_gtaoHistoryTextureResourceState[commonData.m_prevResIdx]);
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
		desc.m_format = m_renderResources->m_gtaoHistoryTextures[commonData.m_curResIdx].getFormat();

		ImageHandle gtaoImageHandle = graph.importImage(desc, m_renderResources->m_gtaoHistoryTextures[commonData.m_curResIdx].getImage(), &m_renderResources->m_gtaoHistoryTextureQueue[commonData.m_curResIdx], &m_renderResources->m_gtaoHistoryTextureResourceState[commonData.m_curResIdx]);
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

	ImageViewHandle voxelSceneImageViewHandle;
	{
		auto &voxelSceneImage = m_renderResources->m_voxelSceneImage;
		ImageDescription desc = {};
		desc.m_name = "Voxel Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = RendererConsts::VOXEL_SCENE_WIDTH;
		desc.m_height = RendererConsts::VOXEL_SCENE_DEPTH;
		desc.m_depth = RendererConsts::VOXEL_SCENE_HEIGHT * RendererConsts::VOXEL_SCENE_CASCADES;
		desc.m_format = voxelSceneImage.getFormat();
		desc.m_imageType = VK_IMAGE_TYPE_3D;

		ImageHandle imageHandle = graph.importImage(desc, voxelSceneImage.getImage(), &m_renderResources->m_voxelSceneImageQueue, &m_renderResources->m_voxelSceneImageResourceState);
		voxelSceneImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });
	}

	ImageViewHandle voxelSceneOpacityImageViewHandle;
	{
		auto &image = m_renderResources->m_voxelSceneOpacityImage;
		ImageDescription desc = {};
		desc.m_name = "Voxel Image";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = RendererConsts::VOXEL_SCENE_WIDTH;
		desc.m_height = RendererConsts::VOXEL_SCENE_DEPTH;
		desc.m_depth = RendererConsts::VOXEL_SCENE_HEIGHT * RendererConsts::VOXEL_SCENE_CASCADES;
		desc.m_format = image.getFormat();
		desc.m_imageType = VK_IMAGE_TYPE_3D;

		ImageHandle imageHandle = graph.importImage(desc, image.getImage(), &m_renderResources->m_voxelSceneOpacityImageQueue, &m_renderResources->m_voxelSceneOpacityImageResourceState);
		voxelSceneOpacityImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });
	}

	ImageViewHandle irradianceVolumeImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = RendererConsts::IRRADIANCE_VOLUME_WIDTH * (RendererConsts::IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH + 2) * RendererConsts::IRRADIANCE_VOLUME_HEIGHT;
		desc.m_height = RendererConsts::IRRADIANCE_VOLUME_DEPTH * (RendererConsts::IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH + 2) * RendererConsts::IRRADIANCE_VOLUME_CASCADES;
		desc.m_depth = 1;
		desc.m_format = m_renderResources->m_irradianceVolumeImage.getFormat();
		desc.m_imageType = VK_IMAGE_TYPE_2D;
	
		ImageHandle imageHandle = 0;
	
		desc.m_name = "Irradiance Volume";
		imageHandle = graph.importImage(desc, m_renderResources->m_irradianceVolumeImage.getImage(), &m_renderResources->m_irradianceVolumeImageQueue, &m_renderResources->m_irradianceVolumeImageResourceState);
		irradianceVolumeImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_2D });
	}

	ImageViewHandle irradianceVolumeDepthImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = RendererConsts::IRRADIANCE_VOLUME_WIDTH * (RendererConsts::IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH + 2) * RendererConsts::IRRADIANCE_VOLUME_HEIGHT;
		desc.m_height = RendererConsts::IRRADIANCE_VOLUME_DEPTH * (RendererConsts::IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH + 2) * RendererConsts::IRRADIANCE_VOLUME_CASCADES;
		desc.m_depth = 1;
		desc.m_format = m_renderResources->m_irradianceVolumeDepthImage.getFormat();
		desc.m_imageType = VK_IMAGE_TYPE_2D;
	
		ImageHandle imageHandle = 0;
	
		desc.m_name = "Irradiance Volume Depth";
		imageHandle = graph.importImage(desc, m_renderResources->m_irradianceVolumeDepthImage.getImage(), &m_renderResources->m_irradianceVolumeDepthImageQueue, &m_renderResources->m_irradianceVolumeDepthImageResourceState);
		irradianceVolumeDepthImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_2D });
	}

	/*ImageViewHandle irradianceVolumeImageViewHandles[3];
	{
		ImageDescription desc = {};
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = RendererConsts::IRRADIANCE_VOLUME_WIDTH;
		desc.m_height = RendererConsts::IRRADIANCE_VOLUME_DEPTH;
		desc.m_depth = RendererConsts::IRRADIANCE_VOLUME_HEIGHT * 2 * RendererConsts::IRRADIANCE_VOLUME_CASCADES;
		desc.m_format = m_renderResources->m_irradianceVolumeXAxisImage.getFormat();
		desc.m_imageType = VK_IMAGE_TYPE_3D;
	
		ImageHandle imageHandle = 0;
	
		desc.m_name = "Irradiance Volume X-Axis Image";
		imageHandle = graph.importImage(desc, m_renderResources->m_irradianceVolumeXAxisImage.getImage(), &m_renderResources->m_irradianceVolumeXAxisImageQueue, &m_renderResources->m_irradianceVolumeXAxisImageResourceState);
		irradianceVolumeImageViewHandles[0] = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });
	
		desc.m_name = "Irradiance Volume Y-Axis Image";
		imageHandle = graph.importImage(desc, m_renderResources->m_irradianceVolumeYAxisImage.getImage(), &m_renderResources->m_irradianceVolumeYAxisImageQueue, &m_renderResources->m_irradianceVolumeYAxisImageResourceState);
		irradianceVolumeImageViewHandles[1] = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });
	
		desc.m_name = "Irradiance Volume Z-Axis Image";
		imageHandle = graph.importImage(desc, m_renderResources->m_irradianceVolumeZAxisImage.getImage(), &m_renderResources->m_irradianceVolumeZAxisImageQueue, &m_renderResources->m_irradianceVolumeZAxisImageResourceState);
		irradianceVolumeImageViewHandles[2] = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });
	}*/

	ImageViewHandle irradianceVolumeAgeImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = RendererConsts::IRRADIANCE_VOLUME_WIDTH;
		desc.m_height = RendererConsts::IRRADIANCE_VOLUME_DEPTH;
		desc.m_depth = RendererConsts::IRRADIANCE_VOLUME_HEIGHT * RendererConsts::IRRADIANCE_VOLUME_CASCADES;
		desc.m_format = m_renderResources->m_irradianceVolumeAgeImage.getFormat();
		desc.m_imageType = VK_IMAGE_TYPE_3D;

		ImageHandle imageHandle = 0;

		desc.m_name = "Irradiance Volume Age Image";
		imageHandle = graph.importImage(desc, m_renderResources->m_irradianceVolumeAgeImage.getImage(), &m_renderResources->m_irradianceVolumeAgeImageQueue, &m_renderResources->m_irradianceVolumeAgeImageResourceState);
		irradianceVolumeAgeImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });
	}

	BufferViewHandle prevLightingQueueBufferViewHandle = 0;
	{
		BufferDescription desc = {};
		desc.m_name = "Prev Lighting Queue Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = 4 * 4098;

		BufferHandle handle = graph.importBuffer(desc, m_renderResources->m_irradianceVolumeQueueBuffers[commonData.m_prevResIdx].getBuffer(), 0, &m_renderResources->m_irradianceVolumeQueueBuffersQueue[commonData.m_prevResIdx], &m_renderResources->m_irradianceVolumeQueueBuffersResourceState[commonData.m_prevResIdx]);
		prevLightingQueueBufferViewHandle = graph.createBufferView({ desc.m_name, handle, 0, desc.m_size });
	}

	BufferViewHandle lightingQueueBufferViewHandle = 0;
	{
		BufferDescription desc = {};
		desc.m_name = "Lighting Queue Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = 4 * 4098;

		BufferHandle handle = graph.importBuffer(desc, m_renderResources->m_irradianceVolumeQueueBuffers[commonData.m_curResIdx].getBuffer(), 0, &m_renderResources->m_irradianceVolumeQueueBuffersQueue[commonData.m_curResIdx], &m_renderResources->m_irradianceVolumeQueueBuffersResourceState[commonData.m_curResIdx]);
		lightingQueueBufferViewHandle = graph.createBufferView({ desc.m_name, handle, 0, desc.m_size });
	}

	BufferViewHandle indirectIrradianceVolumeBufferViewHandle = 0;
	{
		BufferDescription desc = {};
		desc.m_name = "Irradiance Volume Indirect Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = 32;

		indirectIrradianceVolumeBufferViewHandle = graph.createBufferView({ desc.m_name, graph.createBuffer(desc), 0, desc.m_size });
	}

	// create graph managed resources
	ImageViewHandle finalImageViewHandle = VKResourceDefinitions::createFinalImageViewHandle(graph, m_width, m_height);
	ImageViewHandle uvImageViewHandle = VKResourceDefinitions::createUVImageViewHandle(graph, m_width, m_height);
	ImageViewHandle ddxyLengthImageViewHandle = VKResourceDefinitions::createDerivativesLengthImageViewHandle(graph, m_width, m_height);
	ImageViewHandle ddxyRotMaterialIdImageViewHandle = VKResourceDefinitions::createDerivativesRotMaterialIdImageViewHandle(graph, m_width, m_height);
	ImageViewHandle tangentSpaceImageViewHandle = VKResourceDefinitions::createTangentSpaceImageViewHandle(graph, m_width, m_height);
	ImageViewHandle velocityImageViewHandle = VKResourceDefinitions::createVelocityImageViewHandle(graph, m_width, m_height);
	ImageViewHandle lightImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyAccumImageViewHandle = VKResourceDefinitions::createTransparencyAccumImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyTransmittanceImageViewHandle = VKResourceDefinitions::createTransparencyTransmittanceImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyDeltaImageViewHandle = VKResourceDefinitions::createTransparencyDeltaImageViewHandle(graph, m_width, m_height);
	//ImageViewHandle transparencyResultImageViewHandle = VKResourceDefinitions::createLightImageViewHandle(graph, m_width, m_height);
	ImageViewHandle gtaoRawImageViewHandle = VKResourceDefinitions::createGTAOImageViewHandle(graph, m_width, m_height);
	ImageViewHandle gtaoSpatiallyFilteredImageViewHandle = VKResourceDefinitions::createGTAOImageViewHandle(graph, m_width, m_height);
	ImageViewHandle deferredShadowsImageViewHandle = VKResourceDefinitions::createDeferredShadowsImageViewHandle(graph, m_width, m_height);
	ImageViewHandle rayMarchingResultImageViewHandle = VKResourceDefinitions::createRayMarchingResultViewHandle(graph, 64, RendererConsts::IRRADIANCE_VOLUME_QUEUE_CAPACITY);
	ImageViewHandle rayMarchingResultDistanceImageViewHandle = VKResourceDefinitions::createRayMarchingResultViewHandle(graph, 64, RendererConsts::IRRADIANCE_VOLUME_QUEUE_CAPACITY, true);
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

	// depth pyramid
	ImageViewHandle depthPyramidImageViewHandle = 0;
	{
		auto getMipLevelCount = [](uint32_t w, uint32_t h)
		{
			uint32_t result = 1;
			while (w > 1 || h > 1)
			{
				result++;
				w /= 2;
				h /= 2;
			}
			return result;
		};
		const uint32_t levels = getMipLevelCount(m_width / 2, m_height / 2);
	
		ImageHandle depthPyramidImageHandle = VKResourceDefinitions::createDepthPyramidImageHandle(graph, m_width / 2, m_height / 2, levels);
	
		DepthPyramidPass::Data depthPyramidPassData;
		depthPyramidPassData.m_passRecordContext = &passRecordContext;
		depthPyramidPassData.m_inputImageViewHandle = prevDepthImageViewHandle;
		depthPyramidPassData.m_resultImageHandle = depthPyramidImageHandle;
	
		DepthPyramidPass::addToGraph(graph, depthPyramidPassData);
	
		depthPyramidImageViewHandle = graph.createImageView({ "Depth Pyramid Image View", depthPyramidImageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1 } });
	}
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
	deferredShadowsPassData.m_resultImageViewHandle = deferredShadowsImageViewHandle;
	deferredShadowsPassData.m_depthImageViewHandle = depthImageViewHandle;
	deferredShadowsPassData.m_shadowImageViewHandle = shadowImageViewHandle;
	deferredShadowsPassData.m_lightDataBufferInfo = directionalLightDataBufferInfo;
	deferredShadowsPassData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;

	DeferredShadowsPass::addToGraph(graph, deferredShadowsPassData);

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
	lightingPassData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferViewHandle;
	lightingPassData.m_depthImageHandle = depthImageViewHandle;
	lightingPassData.m_uvImageHandle = uvImageViewHandle;
	lightingPassData.m_ddxyLengthImageHandle = ddxyLengthImageViewHandle;
	lightingPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageViewHandle;
	lightingPassData.m_tangentSpaceImageHandle = tangentSpaceImageViewHandle;
	lightingPassData.m_deferredShadowImageViewHandle = deferredShadowsImageViewHandle;
	lightingPassData.m_occlusionImageHandle = gtaoImageViewHandle;
	lightingPassData.m_resultImageHandle = lightImageViewHandle;
	lightingPassData.m_irradianceVolumeImageHandle = irradianceVolumeImageViewHandle;
	lightingPassData.m_irradianceVolumeDepthImageHandle = irradianceVolumeDepthImageViewHandle;

	VKDirectLightingPass::addToGraph(graph, lightingPassData);


	ClearVoxelsPass::Data clearVoxelsPassData;
	clearVoxelsPassData.m_passRecordContext = &passRecordContext;
	clearVoxelsPassData.m_clearImageHandle = voxelSceneOpacityImageViewHandle;
	clearVoxelsPassData.m_clearMode = ClearVoxelsPass::Data::VOXEL_SCENE;

	ClearVoxelsPass::addToGraph(graph, clearVoxelsPassData);


	ClearVoxelsPass::Data clearVolumePassData;
	clearVolumePassData.m_passRecordContext = &passRecordContext;
	clearVolumePassData.m_clearImageHandle = irradianceVolumeAgeImageViewHandle;
	clearVolumePassData.m_clearMode = ClearVoxelsPass::Data::IRRADIANCE_VOLUME;

	ClearVoxelsPass::addToGraph(graph, clearVolumePassData);


	VoxelizationPass::Data voxelizationPassData;
	voxelizationPassData.m_passRecordContext = &passRecordContext;
	voxelizationPassData.m_instanceDataBufferInfo = instanceDataBufferInfo;
	voxelizationPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
	voxelizationPassData.m_transformDataBufferInfo = transformDataBufferInfo;
	voxelizationPassData.m_subMeshInfoBufferInfo = { m_renderResources->m_subMeshDataInfoBuffer.getBuffer(), 0, m_renderResources->m_subMeshDataInfoBuffer.getSize() };
	//voxelizationPassData.m_indicesBufferHandle = opaqueFilteredIndicesBufferViewHandle;
	//voxelizationPassData.m_indirectBufferHandle = opaqueIndirectDrawBufferViewHandle;
	voxelizationPassData.m_voxelSceneImageHandle = voxelSceneImageViewHandle;
	voxelizationPassData.m_voxelSceneOpacityImageHandle = voxelSceneOpacityImageViewHandle;
	voxelizationPassData.m_instanceDataCount = renderData.m_allInstanceDataCount;
	voxelizationPassData.m_instanceData = renderData.m_allInstanceData;
	voxelizationPassData.m_subMeshInfo = m_meshManager->getSubMeshInfo();
	voxelizationPassData.m_shadowImageViewHandle = shadowImageViewHandle;
	voxelizationPassData.m_lightDataBufferInfo = directionalLightDataBufferInfo;
	voxelizationPassData.m_shadowMatricesBufferInfo = shadowMatricesBufferInfo;
	
	VoxelizationPass::addToGraph(graph, voxelizationPassData);


	ScreenSpaceVoxelizationPass::Data ssVoxelPassData;
	ssVoxelPassData.m_passRecordContext = &passRecordContext;
	ssVoxelPassData.m_directionalLightDataBufferInfo = directionalLightDataBufferInfo;
	ssVoxelPassData.m_pointLightDataBufferInfo = pointLightDataBufferInfo;
	ssVoxelPassData.m_pointLightZBinsBufferInfo = pointLightZBinsBufferInfo;
	ssVoxelPassData.m_materialDataBufferInfo = { m_renderResources->m_materialBuffer.getBuffer(), 0, m_renderResources->m_materialBuffer.getSize() };
	ssVoxelPassData.m_pointLightBitMaskBufferHandle = pointLightBitMaskBufferViewHandle;
	ssVoxelPassData.m_depthImageHandle = depthImageViewHandle;
	ssVoxelPassData.m_uvImageHandle = uvImageViewHandle;
	ssVoxelPassData.m_ddxyLengthImageHandle = ddxyLengthImageViewHandle;
	ssVoxelPassData.m_ddxyRotMaterialIdImageHandle = ddxyRotMaterialIdImageViewHandle;
	ssVoxelPassData.m_tangentSpaceImageHandle = tangentSpaceImageViewHandle;
	ssVoxelPassData.m_deferredShadowImageViewHandle = deferredShadowsImageViewHandle;
	ssVoxelPassData.m_voxelSceneImageHandle = voxelSceneImageViewHandle;
	ssVoxelPassData.m_voxelSceneOpacityImageHandle = voxelSceneOpacityImageViewHandle;
	//ssVoxelPassData.m_irradianceVolumeImageHandle = irradianceVolumeImageViewHandle;
	//ssVoxelPassData.m_irradianceVolumeDepthImageHandle = irradianceVolumeDepthImageViewHandle;

	//ScreenSpaceVoxelizationPass::addToGraph(graph, ssVoxelPassData);


	UpdateQueueProbabilityPass::Data updateQueueProbabilityPassData;
	updateQueueProbabilityPassData.m_passRecordContext = &passRecordContext;
	updateQueueProbabilityPassData.m_queueBufferHandle = lightingQueueBufferViewHandle;
	updateQueueProbabilityPassData.m_prevQueueBufferHandle = prevLightingQueueBufferViewHandle;
	updateQueueProbabilityPassData.m_indirectBufferHandle = indirectIrradianceVolumeBufferViewHandle;

	UpdateQueueProbabilityPass::addToGraph(graph, updateQueueProbabilityPassData);


	FillLightingQueuesPass::Data fillLightingQueuesPassData;
	fillLightingQueuesPassData.m_passRecordContext = &passRecordContext;
	fillLightingQueuesPassData.m_ageImageHandle = irradianceVolumeAgeImageViewHandle;
	fillLightingQueuesPassData.m_queueBufferHandle = lightingQueueBufferViewHandle;
	fillLightingQueuesPassData.m_indirectBufferHandle = indirectIrradianceVolumeBufferViewHandle;
	fillLightingQueuesPassData.m_culledBufferInfo = { m_renderResources->m_occlusionCullStatsReadBackBuffers[commonData.m_curResIdx].getBuffer(), 0,m_renderResources->m_occlusionCullStatsReadBackBuffers[commonData.m_curResIdx].getSize() };
	fillLightingQueuesPassData.m_hizImageHandle = depthPyramidImageViewHandle;

	FillLightingQueuesPass::addToGraph(graph, fillLightingQueuesPassData);


	//LightIrradianceVolumePass::Data lightIrradianceVolumePassData;
	//lightIrradianceVolumePassData.m_passRecordContext = &passRecordContext;
	//lightIrradianceVolumePassData.m_ageImageHandle = irradianceVolumeAgeImageViewHandle;
	//lightIrradianceVolumePassData.m_voxelSceneImageHandle = voxelSceneImageViewHandle;
	//lightIrradianceVolumePassData.m_irradianceVolumeImageHandles[0] = irradianceVolumeImageViewHandles[0];
	//lightIrradianceVolumePassData.m_irradianceVolumeImageHandles[1] = irradianceVolumeImageViewHandles[1];
	//lightIrradianceVolumePassData.m_irradianceVolumeImageHandles[2] = irradianceVolumeImageViewHandles[2];
	//lightIrradianceVolumePassData.m_queueBufferHandle = lightingQueueBufferViewHandle;
	//lightIrradianceVolumePassData.m_indirectBufferHandle = indirectIrradianceVolumeBufferViewHandle;
	//lightIrradianceVolumePassData.m_voxelSceneOpacityImageHandle = voxelSceneOpacityImageViewHandle;
	//
	//LightIrradianceVolumePass::addToGraph(graph, lightIrradianceVolumePassData);

	IrradianceVolumeRayMarchingPass::Data irradianceVolumeRayMarchingPassData;
	irradianceVolumeRayMarchingPassData.m_passRecordContext = &passRecordContext;
	irradianceVolumeRayMarchingPassData.m_voxelSceneOpacityImageHandle = voxelSceneOpacityImageViewHandle;
	irradianceVolumeRayMarchingPassData.m_voxelSceneImageHandle = voxelSceneImageViewHandle;
	irradianceVolumeRayMarchingPassData.m_rayMarchingResultImageHandle = rayMarchingResultImageViewHandle;
	irradianceVolumeRayMarchingPassData.m_rayMarchingResultDistanceImageHandle = rayMarchingResultDistanceImageViewHandle;
	irradianceVolumeRayMarchingPassData.m_queueBufferHandle = lightingQueueBufferViewHandle;
	irradianceVolumeRayMarchingPassData.m_indirectBufferHandle = indirectIrradianceVolumeBufferViewHandle;

	IrradianceVolumeRayMarchingPass::addToGraph(graph, irradianceVolumeRayMarchingPassData);
	
	// probe depth
	IrradianceVolumeUpdateProbesPass::Data irradianceVolumeUpdateProbesPassData;
	irradianceVolumeUpdateProbesPassData.m_passRecordContext = &passRecordContext;
	irradianceVolumeUpdateProbesPassData.m_depth = true;
	irradianceVolumeUpdateProbesPassData.m_ageImageHandle = irradianceVolumeAgeImageViewHandle;
	irradianceVolumeUpdateProbesPassData.m_irradianceVolumeImageHandle = irradianceVolumeDepthImageViewHandle;
	irradianceVolumeUpdateProbesPassData.m_rayMarchingResultImageHandle = rayMarchingResultDistanceImageViewHandle;
	irradianceVolumeUpdateProbesPassData.m_queueBufferHandle = lightingQueueBufferViewHandle;
	irradianceVolumeUpdateProbesPassData.m_indirectBufferHandle = indirectIrradianceVolumeBufferViewHandle;
	
	IrradianceVolumeUpdateProbesPass::addToGraph(graph, irradianceVolumeUpdateProbesPassData);
	
	// probe irradiance
	irradianceVolumeUpdateProbesPassData.m_depth = false;
	irradianceVolumeUpdateProbesPassData.m_irradianceVolumeImageHandle = irradianceVolumeImageViewHandle;
	irradianceVolumeUpdateProbesPassData.m_rayMarchingResultImageHandle = rayMarchingResultImageViewHandle;
	
	IrradianceVolumeUpdateProbesPass::addToGraph(graph, irradianceVolumeUpdateProbesPassData);


	/*IrradianceVolumeUpdateACProbesPass::Data irradianceVolumeUpdateACProbesPassData;
	irradianceVolumeUpdateACProbesPassData.m_passRecordContext = &passRecordContext;
	irradianceVolumeUpdateACProbesPassData.m_ageImageHandle = irradianceVolumeAgeImageViewHandle;
	irradianceVolumeUpdateACProbesPassData.m_irradianceVolumeImageHandles[0] = irradianceVolumeImageViewHandles[0];
	irradianceVolumeUpdateACProbesPassData.m_irradianceVolumeImageHandles[1] = irradianceVolumeImageViewHandles[1];
	irradianceVolumeUpdateACProbesPassData.m_irradianceVolumeImageHandles[2] = irradianceVolumeImageViewHandles[2];
	irradianceVolumeUpdateACProbesPassData.m_rayMarchingResultImageHandle = rayMarchingResultImageViewHandle;
	irradianceVolumeUpdateACProbesPassData.m_queueBufferHandle = lightingQueueBufferViewHandle;

	IrradianceVolumeUpdateACProbesPass::addToGraph(graph, irradianceVolumeUpdateACProbesPassData);*/


	VoxelDebugPass::Data voxelDebugData;
	voxelDebugData.m_passRecordContext = &passRecordContext;
	voxelDebugData.m_cascadeIndex = g_debugVoxelCascadeIndex;
	voxelDebugData.m_voxelSceneImageHandle = voxelSceneImageViewHandle;
	voxelDebugData.m_colorImageHandle = lightImageViewHandle;
	voxelDebugData.m_depthImageHandle = depthImageViewHandle;
	voxelDebugData.m_voxelSceneOpacityImageHandle = voxelSceneOpacityImageViewHandle;

	if (g_giVoxelDebugMode == 1)
	{
		VoxelDebugPass::addToGraph(graph, voxelDebugData);
	}


	VoxelDebug2Pass::Data voxelDebug2Data;
	voxelDebug2Data.m_passRecordContext = &passRecordContext;
	voxelDebug2Data.m_cascadeIndex = g_debugVoxelCascadeIndex;
	voxelDebug2Data.m_voxelSceneImageHandle = voxelSceneImageViewHandle;
	voxelDebug2Data.m_colorImageHandle = lightImageViewHandle;
	voxelDebug2Data.m_voxelSceneOpacityImageHandle = voxelSceneOpacityImageViewHandle;

	if (g_giVoxelDebugMode == 2)
	{
		VoxelDebug2Pass::addToGraph(graph, voxelDebug2Data);
	}
	

	IrradianceVolumeDebugPass::Data irradianceVolumeDebugData;
	irradianceVolumeDebugData.m_passRecordContext = &passRecordContext;
	irradianceVolumeDebugData.m_cascadeIndex = g_debugVoxelCascadeIndex;
	irradianceVolumeDebugData.m_showAge = g_giVoxelDebugMode == 4;
	irradianceVolumeDebugData.m_irradianceVolumeImageHandle = irradianceVolumeImageViewHandle;
	irradianceVolumeDebugData.m_irradianceVolumeAgeImageHandle = irradianceVolumeAgeImageViewHandle;
	irradianceVolumeDebugData.m_colorImageHandle = lightImageViewHandle;
	irradianceVolumeDebugData.m_depthImageHandle = depthImageViewHandle;
	
	if (g_giVoxelDebugMode == 3 || g_giVoxelDebugMode == 4)
	{
		IrradianceVolumeDebugPass::addToGraph(graph, irradianceVolumeDebugData);
	}


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


	VkSwapchainKHR swapChain = m_swapChain->get();

	// get swapchain image
	ImageViewHandle swapchainImageViewHandle = 0;
	{
		VkResult result = vkAcquireNextImageKHR(g_context.m_device, swapChain, std::numeric_limits<uint64_t>::max(), m_renderResources->m_swapChainImageAvailableSemaphores[commonData.m_curResIdx], VK_NULL_HANDLE, &m_swapChainImageIndex);

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
	graph.execute(ResourceViewHandle(swapchainImageViewHandle), m_renderResources->m_swapChainImageAvailableSemaphores[commonData.m_curResIdx], &semaphoreCount, &semaphores, ResourceState::PRESENT_IMAGE, QueueType::GRAPHICS);

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
