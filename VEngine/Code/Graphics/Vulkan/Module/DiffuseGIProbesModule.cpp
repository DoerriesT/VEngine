#include "DiffuseGIProbesModule.h"
#include "Graphics/Vulkan/Pass/ClearVoxelsPass.h"
#include "Graphics/Vulkan/Pass/IrradianceVolumeRayMarching2Pass.h"
#include "Graphics/Vulkan/Pass/UpdateQueueProbabilityPass.h"
#include "Graphics/Vulkan/Pass/FillLightingQueuesPass.h"
#include "Graphics/Vulkan/Pass/IrradianceVolumeRayMarching2Pass.h"
#include "Graphics/Vulkan/Pass/IrradianceVolumeUpdateProbesPass.h"
#include "Graphics/Vulkan/Pass/IndirectDiffusePass.h"
#include "Graphics/Vulkan/Pass/IrradianceVolumeDebugPass.h"
#include "Utility/Utility.h"
#include "Graphics/Vulkan/PassRecordContext.h"
#include "Graphics/RenderData.h"
#include "Graphics/Vulkan/VKRenderResources.h"
#include "Graphics/Vulkan/VKBuffer.h"
#include "Graphics/Vulkan/Pass/ReadBackCopyPass.h"

VEngine::DiffuseGIProbesModule::DiffuseGIProbesModule()
{
	// irradiance volume image
	{
		VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageCreateInfo.extent.width = IRRADIANCE_VOLUME_WIDTH * (IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH + 2) * IRRADIANCE_VOLUME_HEIGHT;
		imageCreateInfo.extent.height = IRRADIANCE_VOLUME_DEPTH * (IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH + 2) * IRRADIANCE_VOLUME_CASCADES;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_irradianceVolumeImage, m_irradianceVolumeImageAllocHandle) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image!", EXIT_FAILURE);
		}
	}

	// irradiance volume depth image
	{
		VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R16G16_SFLOAT;
		imageCreateInfo.extent.width = IRRADIANCE_VOLUME_WIDTH * (IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH + 2) * IRRADIANCE_VOLUME_HEIGHT;
		imageCreateInfo.extent.height = IRRADIANCE_VOLUME_DEPTH * (IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH + 2) * IRRADIANCE_VOLUME_CASCADES;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_irradianceVolumeDepthImage, m_irradianceVolumeDepthImageAllocHandle) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image!", EXIT_FAILURE);
		}
	}

	// irradiance volume age image
	{
		VkImageCreateInfo imageCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
		imageCreateInfo.format = VK_FORMAT_R8_UNORM;
		imageCreateInfo.extent.width = IRRADIANCE_VOLUME_WIDTH;
		imageCreateInfo.extent.height = IRRADIANCE_VOLUME_DEPTH; // width and depth are same size
		imageCreateInfo.extent.depth = IRRADIANCE_VOLUME_HEIGHT * IRRADIANCE_VOLUME_CASCADES; // keep all cascades in same image by extending image depth
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (g_context.m_allocator.createImage(allocCreateInfo, imageCreateInfo, m_irradianceVolumeAgeImage, m_irradianceVolumeAgeImageAllocHandle) != VK_SUCCESS)
		{
			Utility::fatalExit("Failed to create image!", EXIT_FAILURE);
		}
	}

	uint32_t queueFamilyIndices[] =
	{
		g_context.m_queueFamilyIndices.m_graphicsFamily,
		g_context.m_queueFamilyIndices.m_computeFamily,
		g_context.m_queueFamilyIndices.m_transferFamily
	};

	// lighting queue buffers
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = 4098 * 4;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			if (g_context.m_allocator.createBuffer(allocCreateInfo, bufferCreateInfo, m_probeQueueBuffers[i], m_probeQueueBuffersAllocHandles[i]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create buffer!", EXIT_FAILURE);
			}
			m_probeQueueBuffersQueue[i] = RenderGraph::undefinedQueue;
			m_probeQueueBuffersResourceState[i] = ResourceState::UNDEFINED;
		}
	}

	// culled probes readback buffers
	{
		VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferCreateInfo.size = sizeof(uint32_t);
		bufferCreateInfo.size = bufferCreateInfo.size < 32 ? 32 : bufferCreateInfo.size;
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCreateInfo.queueFamilyIndexCount = 3;
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;

		VKAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.m_requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

		for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
		{
			if (g_context.m_allocator.createBuffer(allocCreateInfo, bufferCreateInfo, m_culledProbesReadBackBuffers[i], m_culledProbesReadBackBufferAllocHandles[i]) != VK_SUCCESS)
			{
				Utility::fatalExit("Failed to create buffer!", EXIT_FAILURE);
			}
		}
	}
}

VEngine::DiffuseGIProbesModule::~DiffuseGIProbesModule()
{
	g_context.m_allocator.destroyImage(m_irradianceVolumeImage, m_irradianceVolumeImageAllocHandle);
	g_context.m_allocator.destroyImage(m_irradianceVolumeDepthImage, m_irradianceVolumeDepthImageAllocHandle);
	g_context.m_allocator.destroyImage(m_irradianceVolumeAgeImage, m_irradianceVolumeAgeImageAllocHandle);

	for (size_t i = 0; i < RendererConsts::FRAMES_IN_FLIGHT; ++i)
	{
		g_context.m_allocator.destroyBuffer(m_probeQueueBuffers[i], m_probeQueueBuffersAllocHandles[i]);
	}
}

void VEngine::DiffuseGIProbesModule::readBackCulledProbesCount(uint32_t currentResourceIndex)
{
	const auto allocHandle = m_culledProbesReadBackBufferAllocHandles[currentResourceIndex];
	const auto allocInfo = g_context.m_allocator.getAllocationInfo(allocHandle);
	uint32_t *data;
	g_context.m_allocator.mapMemory(allocHandle, (void **)&data);

	VkMappedMemoryRange range{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
	range.memory = allocInfo.m_memory;
	range.offset = Utility::alignDown(allocInfo.m_offset, g_context.m_properties.limits.nonCoherentAtomSize);
	range.size = Utility::alignUp(sizeof(uint32_t), g_context.m_properties.limits.nonCoherentAtomSize);

	vkInvalidateMappedMemoryRanges(g_context.m_device, 1, &range);
	m_culledProbeCount = *data;

	g_context.m_allocator.unmapMemory(m_culledProbesReadBackBufferAllocHandles[currentResourceIndex]);
}

void VEngine::DiffuseGIProbesModule::updateResourceHandles(RenderGraph &graph)
{
	// irradiance volume image
	{
		ImageDescription desc = {};
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = IRRADIANCE_VOLUME_WIDTH * (IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH + 2) * IRRADIANCE_VOLUME_HEIGHT;
		desc.m_height = IRRADIANCE_VOLUME_DEPTH * (IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH + 2) * IRRADIANCE_VOLUME_CASCADES;
		desc.m_depth = 1;
		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		desc.m_imageType = VK_IMAGE_TYPE_2D;
		desc.m_name = "Irradiance Volume";

		ImageHandle imageHandle = graph.importImage(desc, m_irradianceVolumeImage, &m_irradianceVolumeImageQueue, &m_irradianceVolumeImageResourceState);
		m_irradianceVolumeImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_2D });
	}

	// irradiance volume depth image
	{
		ImageDescription desc = {};
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = IRRADIANCE_VOLUME_WIDTH * (IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH + 2) * IRRADIANCE_VOLUME_HEIGHT;
		desc.m_height = IRRADIANCE_VOLUME_DEPTH * (IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH + 2) * IRRADIANCE_VOLUME_CASCADES;
		desc.m_depth = 1;
		desc.m_format = VK_FORMAT_R16G16_SFLOAT;
		desc.m_imageType = VK_IMAGE_TYPE_2D;
		desc.m_name = "Irradiance Volume Depth";

		ImageHandle imageHandle = graph.importImage(desc, m_irradianceVolumeDepthImage, &m_irradianceVolumeDepthImageQueue, &m_irradianceVolumeDepthImageResourceState);
		m_irradianceVolumeDepthImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_2D });
	}

	// irradiance volume age image
	{
		ImageDescription desc = {};
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = IRRADIANCE_VOLUME_WIDTH;
		desc.m_height = IRRADIANCE_VOLUME_DEPTH;
		desc.m_depth = IRRADIANCE_VOLUME_HEIGHT * IRRADIANCE_VOLUME_CASCADES;
		desc.m_format = VK_FORMAT_R8_UNORM;
		desc.m_imageType = VK_IMAGE_TYPE_3D;
		desc.m_name = "Irradiance Volume Age Image";

		ImageHandle imageHandle = graph.importImage(desc, m_irradianceVolumeAgeImage, &m_irradianceVolumeAgeImageQueue, &m_irradianceVolumeAgeImageResourceState);
		m_irradianceVolumeAgeImageViewHandle = graph.createImageView({ desc.m_name, imageHandle, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }, VK_IMAGE_VIEW_TYPE_3D });
	}
}

void VEngine::DiffuseGIProbesModule::addProbeUpdateToGraph(RenderGraph &graph, const Data &data)
{
	auto &commonData = *data.m_passRecordContext->m_commonRenderData;
	auto &renderResources = *data.m_passRecordContext->m_renderResources;

	BufferViewHandle prevLightingQueueBufferViewHandle = 0;
	{
		BufferDescription desc = {};
		desc.m_name = "Prev Lighting Queue Buffer";
		desc.m_concurrent = true;
		desc.m_clear = false;
		desc.m_clearValue.m_bufferClearValue = 0;
		desc.m_size = 4 * 4098;

		BufferHandle handle = graph.importBuffer(desc, m_probeQueueBuffers[commonData.m_prevResIdx], 0, &m_probeQueueBuffersQueue[commonData.m_prevResIdx], &m_probeQueueBuffersResourceState[commonData.m_prevResIdx]);
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

		BufferHandle handle = graph.importBuffer(desc, m_probeQueueBuffers[commonData.m_curResIdx], 0, &m_probeQueueBuffersQueue[commonData.m_curResIdx], &m_probeQueueBuffersResourceState[commonData.m_curResIdx]);
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

	ImageViewHandle rayMarchingResultImageViewHandle;
	ImageViewHandle rayMarchingResultDistanceImageViewHandle;
	{
		ImageDescription desc = {};
		desc.m_name = "Ray Marching Result";
		desc.m_concurrent = false;
		desc.m_clear = false;
		desc.m_clearValue.m_imageClearValue = {};
		desc.m_width = IRRADIANCE_VOLUME_RAY_MARCHING_RAY_COUNT;
		desc.m_height = IRRADIANCE_VOLUME_QUEUE_CAPACITY;
		desc.m_layers = 1;
		desc.m_levels = 1;
		desc.m_samples = 1;

		desc.m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
		rayMarchingResultImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });

		desc.m_format = VK_FORMAT_R16G16_SFLOAT;
		rayMarchingResultDistanceImageViewHandle = graph.createImageView({ desc.m_name, graph.createImage(desc), { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } });
	}

	// mark probes as new if the volume moved
	IrradianceVolumeClearProbesPass::Data clearVolumePassData;
	clearVolumePassData.m_passRecordContext = data.m_passRecordContext;
	clearVolumePassData.m_irradianceVolumeAgeImageHandle = m_irradianceVolumeAgeImageViewHandle;

	IrradianceVolumeClearProbesPass::addToGraph(graph, clearVolumePassData);


	// update probability if probes being queued for relighting
	UpdateQueueProbabilityPass::Data updateQueueProbabilityPassData;
	updateQueueProbabilityPassData.m_passRecordContext = data.m_passRecordContext;
	updateQueueProbabilityPassData.m_queueBufferHandle = lightingQueueBufferViewHandle;
	updateQueueProbabilityPassData.m_prevQueueBufferHandle = prevLightingQueueBufferViewHandle;
	updateQueueProbabilityPassData.m_indirectBufferHandle = indirectIrradianceVolumeBufferViewHandle;

	UpdateQueueProbabilityPass::addToGraph(graph, updateQueueProbabilityPassData);


	// fill lighting queue
	FillLightingQueuesPass::Data fillLightingQueuesPassData;
	fillLightingQueuesPassData.m_passRecordContext = data.m_passRecordContext;
	fillLightingQueuesPassData.m_ageImageHandle = m_irradianceVolumeAgeImageViewHandle;
	fillLightingQueuesPassData.m_queueBufferHandle = lightingQueueBufferViewHandle;
	fillLightingQueuesPassData.m_indirectBufferHandle = indirectIrradianceVolumeBufferViewHandle;
	fillLightingQueuesPassData.m_culledBufferInfo = { m_culledProbesReadBackBuffers[commonData.m_curResIdx], 0, 4 };
	fillLightingQueuesPassData.m_hizImageHandle = data.m_depthPyramidImageViewHandle;

	FillLightingQueuesPass::addToGraph(graph, fillLightingQueuesPassData);


	// ray marching
	IrradianceVolumeRayMarching2Pass::Data irradianceVolumeRayMarching2PassData;
	irradianceVolumeRayMarching2PassData.m_passRecordContext = data.m_passRecordContext;
	irradianceVolumeRayMarching2PassData.m_brickPtrImageHandle = data.m_brickPointerImageViewHandle;
	irradianceVolumeRayMarching2PassData.m_binVisBricksBufferHandle = data.m_binVisBricksBufferViewHandle;
	irradianceVolumeRayMarching2PassData.m_colorBricksBufferHandle = data.m_colorBricksBufferViewHandle;
	irradianceVolumeRayMarching2PassData.m_rayMarchingResultImageHandle = rayMarchingResultImageViewHandle;
	irradianceVolumeRayMarching2PassData.m_rayMarchingResultDistanceImageHandle = rayMarchingResultDistanceImageViewHandle;
	irradianceVolumeRayMarching2PassData.m_queueBufferHandle = lightingQueueBufferViewHandle;
	irradianceVolumeRayMarching2PassData.m_indirectBufferHandle = indirectIrradianceVolumeBufferViewHandle;

	IrradianceVolumeRayMarching2Pass::addToGraph(graph, irradianceVolumeRayMarching2PassData);


	// update probe depth
	IrradianceVolumeUpdateProbesPass::Data irradianceVolumeUpdateProbesPassData;
	irradianceVolumeUpdateProbesPassData.m_passRecordContext = data.m_passRecordContext;
	irradianceVolumeUpdateProbesPassData.m_depth = true;
	irradianceVolumeUpdateProbesPassData.m_ageImageHandle = m_irradianceVolumeAgeImageViewHandle;
	irradianceVolumeUpdateProbesPassData.m_irradianceVolumeImageHandle = m_irradianceVolumeDepthImageViewHandle;
	irradianceVolumeUpdateProbesPassData.m_rayMarchingResultImageHandle = rayMarchingResultDistanceImageViewHandle;
	irradianceVolumeUpdateProbesPassData.m_queueBufferHandle = lightingQueueBufferViewHandle;
	irradianceVolumeUpdateProbesPassData.m_indirectBufferHandle = indirectIrradianceVolumeBufferViewHandle;

	IrradianceVolumeUpdateProbesPass::addToGraph(graph, irradianceVolumeUpdateProbesPassData);

	// update probe irradiance
	irradianceVolumeUpdateProbesPassData.m_depth = false;
	irradianceVolumeUpdateProbesPassData.m_irradianceVolumeImageHandle = m_irradianceVolumeImageViewHandle;
	irradianceVolumeUpdateProbesPassData.m_rayMarchingResultImageHandle = rayMarchingResultImageViewHandle;

	IrradianceVolumeUpdateProbesPass::addToGraph(graph, irradianceVolumeUpdateProbesPassData);
}

void VEngine::DiffuseGIProbesModule::addDebugVisualizationToGraph(RenderGraph &graph, const DebugVisualizationData &data)
{
	IrradianceVolumeDebugPass::Data irradianceVolumeDebugData;
	irradianceVolumeDebugData.m_passRecordContext = data.m_passRecordContext;
	irradianceVolumeDebugData.m_cascadeIndex = data.m_cascadeIndex;
	irradianceVolumeDebugData.m_showAge = data.m_showAge;
	irradianceVolumeDebugData.m_irradianceVolumeImageHandle = m_irradianceVolumeImageViewHandle;
	irradianceVolumeDebugData.m_irradianceVolumeAgeImageHandle = m_irradianceVolumeAgeImageViewHandle;
	irradianceVolumeDebugData.m_colorImageHandle = data.m_colorImageViewHandle;
	irradianceVolumeDebugData.m_depthImageHandle = data.m_depthImageViewHandle;

	IrradianceVolumeDebugPass::addToGraph(graph, irradianceVolumeDebugData);
}

VEngine::ImageViewHandle VEngine::DiffuseGIProbesModule::getIrradianceVolumeImageViewHandle()
{
	return m_irradianceVolumeImageViewHandle;
}

VEngine::ImageViewHandle VEngine::DiffuseGIProbesModule::getIrradianceVolumeDepthImageViewHandle()
{
	return m_irradianceVolumeDepthImageViewHandle;
}

uint32_t VEngine::DiffuseGIProbesModule::getCulledProbeCount() const
{
	return m_culledProbeCount;
}
